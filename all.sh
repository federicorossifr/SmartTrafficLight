cd G1;
make -j4 TARGET=sky clean;
make -j4 TARGET=sky;
cd ../G2;
make -j4 TARGET=sky clean;
make -j4 TARGET=sky;
cd ../TL1;
make -j4 TARGET=sky clean;
make -j4 TARGET=sky;
cd ../TL2;
make -j4 TARGET=sky clean;
make -j4 TARGET=sky;

