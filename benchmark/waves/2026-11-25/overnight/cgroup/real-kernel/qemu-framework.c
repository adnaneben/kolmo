extern void qemu_init(int, char **);
extern void qemu_main_loop(void);
extern void qemu_cleanup(int);
int main(int argc,char **argv){qemu_init(argc,argv);qemu_main_loop();qemu_cleanup(0);return 0;}
