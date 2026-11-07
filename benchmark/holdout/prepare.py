"""Load public inputs without rewriting records and verify release archive provenance."""
import csv
import re
import tarfile
import zipfile
import shutil
from common import *

def file_map(path,prefix):
    with tarfile.open(path) as ar:
        return {m.name[len(prefix):]:hashlib.sha256(ar.extractfile(m).read()).hexdigest() for m in ar if m.isfile() and m.name.startswith(prefix) and m.name[len(prefix):].startswith(('src/','vendor/'))}

def provenance(download):
    brew=download/'brew.tar.gz'; pip=download/'kolmoc.tar.gz'; cargo=download/'kolmo.crate'
    sdist=next(f for f in json.loads((download/'pypi.json').read_text())['urls'] if f['packagetype']=='sdist')
    crate=next(j for j in map(json.loads,(download/'crates-index').read_text().splitlines()) if j['vers']=='1.0.0')
    formula=(download/'formula.rb').read_text()
    assert digest(pip)==sdist['digests']['sha256'] and digest(cargo)==crate['cksum']
    assert digest(brew)==re.search(r'sha256 "([0-9a-f]{64})"',formula)[1]
    core=file_map(brew,'kolmo-1.0.0/')
    records={}
    for name,archive,prefix in [('pip',pip,'kolmoc-1.0.0/native/'),('cargo',cargo,'kolmo-1.0.0/native/')]:
        actual=file_map(archive,prefix)
        assert actual and all(core.get(k)==v for k,v in actual.items())
        assert all(k in actual for k in core if k.startswith('src/'))
        records[name]=dict(files_checked=len(actual),native_byte_mismatches=0,omitted_from_root=sorted(core.keys()-actual.keys()),archive_sha256=digest(archive))
    with tarfile.open(cargo) as ar:
        records['cargo']['vcs']=json.load(ar.extractfile('kolmo-1.0.0/.cargo_vcs_info.json'))
    records['brew']=dict(archive_sha256=digest(brew),source_files=len(core),tag='v1.0.0',peeled_tag='7514f9415e99a8da82d14b42922e60b7ae647d00',installed_cli_sha256=digest(BIN/'release'))
    save(OUT/'release-provenance.json',records)

def prepare():
    download=WORK/'downloads'
    provenance(download)
    sources={'power':('power.zip','household_power_consumption.txt',';','https://archive.ics.uci.edu/static/public/235/individual+household+electric+power+consumption.zip'),
             'rides':('rides.zip','202201-divvy-tripdata.csv',',','https://divvy-tripdata.s3.amazonaws.com/202201-divvy-tripdata.zip'),
             'quakes':('quakes.csv',None,',','https://earthquake.usgs.gov/fdsnws/event/1/query?format=csv&starttime=2020-01-01T00:00:00&endtime=2020-02-01T00:00:00&orderby=time-asc&limit=20000')}
    manifest={}
    for name,(filename,member,sep,url) in sources.items():
        archive=download/filename
        source=WORK/(name+'.source')
        if member:
            with zipfile.ZipFile(archive) as z,z.open(member) as src, source.open('wb') as out:
                shutil.copyfileobj(src,out)
        else:
            shutil.copyfile(archive,source)
        with source.open('rb') as stream:
            rows=sum(1 for _ in stream)-1
        # All sources have one logical record per line. Validate before splitting.
        with source.open(encoding='utf-8-sig',newline='') as stream:
            reader=csv.reader(stream,delimiter=sep,strict=True)
            assert sum(1 for _ in reader)==rows+1
        train=WORK/(name+'.train');test=WORK/(name+'.test')
        with source.open('rb') as raw,train.open('wb') as trainer,test.open('wb') as scored:
            header=next(raw);trainer.write(header);scored.write(header);train_rows=0
            for i,line in enumerate(raw):
                if i>=rows//4:
                    scored.write(line)
                elif train_rows==i and trainer.tell()+len(line)<=4*1024**2:
                    trainer.write(line);train_rows+=1
        manifest[name]=dict(url=url,archive_sha256=digest(archive),source_sha256=digest(source),source_rows=rows,
                            fields_delimiter=sep,train_rows=train_rows,train_bytes=train.stat().st_size,
                            train_sha256=digest(train),test_rows=rows-rows//4,test_bytes=test.stat().st_size,test_sha256=digest(test))
    save(OUT/'inputs.json',manifest)

if __name__=='__main__':prepare()
