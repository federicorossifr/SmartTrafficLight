chmod -R u+rwx *
cd G1;
make -j5 TARGET=sky clean;
make -j5 TARGET=sky;
cd ../G2;
make -j5 TARGET=sky clean;
make -j5 TARGET=sky;
cd ../TL;
make -j5 TARGET=sky clean;
make -j5 TARGET=sky;

