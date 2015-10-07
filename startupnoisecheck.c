void startupNOISE(uint16_t *m, uint32_t mlen) {
uint32_t maxsignal, minsignal, delta, lev1, lev2, lev3, lev4, maxlev, levstep;
uint32_t noise;

maxsignal = 0;
minsignal = 99999;

//сканирование буффера длиной mlen
for (noise = 0; noise < mlen; noise++) {
if(maxsignal<m[noise]) maxsignal = m[noise];
if(minsignal>m[noise]) minsignal = m[noise];
} //end for

printf("\nMax Signal=%d  Min Signal=%d\n\n", maxsignal, minsignal );

delta = maxsignal / 4;

lev1 = 0;
lev2 = 0;
lev3 = 0;
lev4 = 0;
//сканирование буффера длиной mlen
for (noise = 0; noise < mlen; noise++) {
if((minsignal < m[noise]) && (m[noise] < minsignal+delta)) lev1++;
if((minsignal+delta) < m[noise] && (m[noise] < minsignal+2*delta)) lev2++;
if((minsignal+2*delta) < m[noise] && (m[noise] < minsignal+3*delta)) lev3++;
if((minsignal+3*delta) < m[noise] && (m[noise] < maxsignal)) lev4++;
} //end for

maxlev = 0;
if(lev1>maxlev) maxlev = lev1;
if(lev2>maxlev) maxlev = lev2;
if(lev3>maxlev) maxlev = lev3;
if(lev4>maxlev) maxlev = lev4;

levstep = maxlev / 10;

for (noise = 0; noise < lev1; noise+=levstep) {
printf("X");
}
printf(" Level1=%d - %d\n", minsignal+delta, lev1);

for (noise = 0; noise < lev2; noise+=levstep) {
printf("X");
}
printf(" Level2=%d - %d\n", minsignal+2*delta, lev2);

for (noise = 0; noise < lev3; noise+=levstep) {
printf("X");
}
printf(" Level3=%d - %d\n", minsignal+3*delta, lev3);


for (noise = 0; noise < lev4; noise+=levstep) {
printf("X");
}
printf(" Level4=%d - %d\n", maxsignal, lev4);


} // end startupNOISE