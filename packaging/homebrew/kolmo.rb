class Kolmo < Formula
  desc "Structure-aware lossless compressor for tables, logs and scientific data"
  homepage "https://github.com/adnaneben/kolmo"
  # After you cut the GitHub release, set url to its source tarball and paste the sha256:
  #   curl -sL https://github.com/adnaneben/kolmo/archive/refs/tags/v1.0.0.tar.gz | shasum -a 256
  url "https://github.com/adnaneben/kolmo/archive/refs/tags/v1.0.0.tar.gz"
  sha256 "REPLACE_WITH_TARBALL_SHA256"
  license "Apache-2.0"
  head "https://github.com/adnaneben/kolmo.git", branch: "main"

  depends_on "xz" # liblzma, for the max/ultra tiers

  def install
    system "make"
    bin.install "build/kolmo"
  end

  test do
    # round-trip a file and confirm it comes back byte-identical
    (testpath/"in.csv").write (1..500).map { |i| "row,#{i},#{i * 2},ok\n" }.join
    system bin/"kolmo", "-m", "fast", "in.csv", "out.czn"
    system bin/"kolmo", "-d", "out.czn", "back.csv"
    assert_equal (testpath/"in.csv").read, (testpath/"back.csv").read
  end
end
