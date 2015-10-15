//возвращает уровень шума
uint32_t startupNOISE(uint16_t *m, uint32_t mlen, char* ntimestr ) {
uint32_t maxsignal, delta, noiselevel;
uint32_t levels[10];
uint32_t noise;
uint i;
uint32_t howvals, mediana, linotkl;//, dispersion, srkvotkl;
// time_t start,end;
// double dif;

uint deltares = 10; //10 уровней анализа шума

for (i = 0; i< deltares; i++)
{
levels[i] = 0;
}

maxsignal = 0;
// minsignal = 0;

mediana = 0;
howvals = 0;
//сканирование буффера длиной mlen, смотрим каждый 100-ый элемент для ускорения
for (noise = 0; noise < mlen; noise+=100) {
if(maxsignal<m[noise]) maxsignal = m[noise];

//значения для среднего отклонения
mediana+=m[noise];
howvals++;
} //end for


//++++++++++++++++++++++++++++++++++++++++++++
mediana = mediana / howvals;

//сканирование буффера длиной mlen, смотрим каждый 100-ый элемент для ускорения
linotkl = 0;
for (noise = 0; noise < mlen; noise+=100) {
linotkl+= abs(m[noise] - mediana);
} //end for

//расчет среднего линейного отклонения
linotkl = linotkl / howvals;


// //сканирование буффера длиной mlen, смотрим каждый 100-ый элемент для ускорения
// dispersion = 0;
// for (noise = 0; noise < mlen; noise+=100) {
// dispersion+= (m[noise] - mediana)^2;
// } //end for

// //дисперсия
// dispersion = dispersion / howvals;

// //среднеквадратичное отклоненение
// srkvotkl = sqrt(dispersion);


// printf("%s - MED = %05d  LINOTKL=%05d  DISP=%05d  SRKVOTKL=%05d\n", ntimestr, mediana, linotkl, dispersion, srkvotkl);
//++++++++++++++++++++++++++++++++++++++++++++





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
	//разница между спектрами более чем 4 раза считаем за порог шума
	if( levels[i] > (levels[i+1]*3) ) { noiselevel = delta*(i+1); break; }					// 4 - PARAMETER!!!!!!!!!!!
	//если такого порога нет то
	else noiselevel = (maxsignal / 2);
}

	//чем уже вариативность сигнала тем больше дельт надо для превышения 1 над шумом
	//чем ближе Noiselevel к Maxsignal тем хуже
	
	//ВРЕМЕННО ПОСТАВИМ на 3!!!!!!!!!!!!
	noiselevel+=delta*2;																	// 3 - PARAMETER!!!!!!!!!!!

	//ограничение на шумоподобный сигнал ???
	// if(noiselevel<750) noiselevel = 750;													// 750 - PARAMETER!!!!!!!!!!!

	//брать среднее значение из нескольких последних??


// printf("%s - Noise level = %05d (%05d) - Max sig %05d  Delta %05d  MED = %05d  LINOTKL=%05d\r", ntimestr, noiselevel, mediana+linotkl, maxsignal, delta, mediana, linotkl);


return noiselevel;


} // end startupNOISE