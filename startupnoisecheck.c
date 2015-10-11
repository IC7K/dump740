//возвращает уровень шума
uint32_t startupNOISE(uint16_t *m, uint32_t mlen) {
uint32_t maxsignal, delta, noiselevel;
uint32_t levels[10];
uint32_t noise;
uint i;
// time_t start,end;
// double dif;

uint deltares = 10; //10 уровней анализа шума

char timestr[20]/*, filestr[20]*/;
struct timeval tp;
gettimeofday(&tp, 0);
time_t curtime = tp.tv_sec;
struct tm *t = localtime(&curtime);
sprintf(timestr, "%02d:%02d:%02d.%03d", (int) t->tm_hour, (int) t->tm_min, (int) t->tm_sec, (int) tp.tv_usec/1000);


for (i = 0; i< deltares; i++)
{
levels[i] = 0;
}

maxsignal = 0;
// minsignal = 0;

//сканирование буффера длиной mlen, смотрим каждый 100-ый элемент для ускорения
for (noise = 0; noise < mlen; noise+=100) {
if(maxsignal<m[noise]) maxsignal = m[noise];
// if(minsignal>m[noise]) minsignal = m[noise];
} //end for

//насколько разбиваем уровень сигнала
delta = maxsignal / deltares;



//сканирование буффера длиной mlen, смотрим каждый 100-ый элемент для ускорения
for (noise = 0; noise < mlen; noise+=100) {

for (i = 0; i< deltares; i++)
{
	if(( (delta*i) < m[noise]) && (m[noise] < (delta*(i+1)) )) levels[i]++;
} // end for i

} //end for noise



noiselevel = 0;
for (i = 0; i < (deltares-1); i++)
{
	//разница между спектрами более чем 3 раза считаем за порог шума
	//ставим уровень на пару delta выше
	if( levels[i] > (levels[i+1]*2) ) { noiselevel = delta*(i+1); break; }
	//если такого порога нет то
	else noiselevel = (maxsignal / 2);
}

	noiselevel+=delta;
// time (&end);
// dif = difftime (end,start);

// printf("Noise level = %d - Before %d  After %d  - Time=%.9lf\n\n", noiselevel, levels[i], levels[i+1], dif);

printf("%s - Noise level = %05d - Max sig %05d  Delta %05d\r", timestr, noiselevel, maxsignal, delta);

return noiselevel;

// maxlev = 0;
// if(lev1>maxlev) maxlev = lev1;
// if(lev2>maxlev) maxlev = lev2;
// if(lev3>maxlev) maxlev = lev3;
// if(lev4>maxlev) maxlev = lev4;

// levstep = maxlev / 10;

// for (noise = 0; noise < lev1; noise+=levstep) {
// printf("X");
// }
// printf(" Level1=%d - %d\n", minsignal+delta, lev1);

// for (noise = 0; noise < lev2; noise+=levstep) {
// printf("X");
// }
// printf(" Level2=%d - %d\n", minsignal+2*delta, lev2);

// for (noise = 0; noise < lev3; noise+=levstep) {
// printf("X");
// }
// printf(" Level3=%d - %d\n", minsignal+3*delta, lev3);


// for (noise = 0; noise < lev4; noise+=levstep) {
// printf("X");
// }
// printf(" Level4=%d - %d\n", maxsignal, lev4);


} // end startupNOISE