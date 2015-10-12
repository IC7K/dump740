void oscilloscope(uint16_t *m, uint16_t *ideal, uint32_t pkkoffs,  uint32_t osclen, uint32_t pkkpulselevel) {
uint i, j;
uint32_t maxsignal, delta;
uint deltares = 10; //10 уровней анализа
uint pulselevel;

maxsignal = 0;


//сканирование буффера длиной mlen, смотрим каждый 100-ый элемент для ускорения
for (i = 0; i < osclen; i++) {
if(maxsignal<m[i]) maxsignal = m[i];
// if(minsignal>m[noise]) minsignal = m[noise];
} //end for

if (maxsignal < pkkpulselevel) maxsignal = pkkpulselevel;
//насколько разбиваем уровень сигнала
delta = maxsignal / deltares;

if(delta==0) delta=1;

pulselevel = pkkpulselevel/delta;

printf("\n");
printf("Noise Lev=%d  Max=%d  VResolution=%d HLen=%d\n", pkkpulselevel, maxsignal, delta, osclen);

for (i = (deltares); (i+1)>0; i--) //10 строк по вертикали
{
	//сканирование буффера длиной osclen
	for (j = 0; j < osclen; j++) {
		if( (delta*i) < m[pkkoffs+j] ) if( i == pulselevel ) printf("-"); else if (ideal[j] != 0) printf("|"); else printf("*"); else printf(" ");
	} // end for j
	printf("\n");

} //end for i

	for (j = 0; j < osclen; j++) printf("_");
	printf("\n");

}


/*    
    Всего стандартом предусмотрено шесть координатных кодов: ОК1-ОК6.
    Они образуются двумя периодами: РК1 и РК3.
    Кодовый интервал между ними определяет содержание ответа и может быть: 14, 11, 18, 22, 9 и 6мкс.
    На привычные нам запросы ЗК1-ЗК4 высылаются ответы с первыми четырьмя значениями интервала.
    Между РК1 и РК3 может посылаться импульс РК2, который кодирует два дополнительных сообщения.
    Когда РК2 опережает РК3 на 6мкс - передается сигнал "Бедствие".
    Если РК2 отстает от РК1 на 6мкс - передается сигнал "Шасси выпущено".
    Длительность периода неважна - определяется только период времени между началами импульсов.
    
    ЕСЛИ СЭМПЛИРОВАНИЕ СИГНАЛА 0,5мкс
    ТО ВРЕМЕННаЯ РАСКЛАДКА БУДЕТ ТАКОЙ ([0,1,..] – номер значения в массиве данных)
    
    [0]   начало 1-го периода т=0 мкс        ------------------ PK1 начало
    [1]   начало 2-го периода т=0,5 мкс
    [2]   начало 3-го периода т=1 мкс
    [3]   начало 4-го периода т=1,5 мкс     
    [4]   начало 5-го периода т=2 мкс  
    [5]   начало 6-го периода т=2,5 мкс  
    [6]   начало 7-го периода т=3 мкс          
    [7]   начало 8-го периода т=3,5 мкс
    [8]   начало 9-го периода т=4 мкс     
    [9]   начало 10-го периода т=4,5 мкс  
    [10]  начало 11-го периода т=5 мкс         ------------------------ РК2 для ОК2 - ОК6 (бедствие)        
    [11]  начало 12-го периода т=5,5 мкс       
    [12]  начало 13-го периода т=6 мкс         ------------ РК2 начало после РК1 - ОК5 (шасси выпущено)
    [13]  начало 14-го периода т=6,5 мкс       
    [14]  начало 15-го периода т=7 мкс         
    [15]  начало 16-го периода т=7,5 мкс       
    [16]  начало 17-го периода т=8 мкс         ------------------------ РК2 для ОК1 - ОК6 (бедствие) 
    [17]  начало 18-го периода т=8,5 мкс       
    [18]  начало 19-го периода т=9 мкс          
    [19]  начало 20-го периода т=9,5 мкс       
    [20]  начало 21-го периода т=10 мкс                   
    [21]  начало 22-го периода т=10,5 мкс      
    [22]  начало 23-го периода т=11 мкс        ------------------ **** PK3 начало - ОК2 (для ЗК2) 
    [23]  начало 24-го периода т=11,5 мкс      
    [24]  начало 25-го периода т=12 мкс        ------------------------ РК2 для ОК3 - ОК6 (бедствие)     
    [25]  начало 26-го периода т=12,5 мкс      
    [26]  начало 27-го периода т=13 мкс        
    [27]  начало 28-го периода т=13,5 мкс      
    [28]  начало 29-го периода т=14 мкс        ------------------ **** PK3 начало - ОК1 (для ЗК1) 
    [29]  начало 30-го периода т=14,5 мкс  
    [30]  начало 31-го периода т=15 мкс 
    [31]  начало 32-го периода т=15,5 мкс
    [32]  начало 33-го периода т=16 мкс        ------------------------ РК2 для ОК4 - ОК6 (бедствие) 
    [33]  начало 34-го периода т=16,5 мкс     
    [34]  начало 35-го периода т=17 мкс  
    [35]  начало 36-го периода т=17,5 мкс  
    [36]  начало 37-го периода т=18 мкс        ------------------ **** PK3 начало - ОК3 (для ЗК3) 
    [37]  начало 38-го периода т=18,5 мкс
    [38]  начало 39-го периода т=19 мкс     
    [39]  начало 40-го периода т=19,5 мкс  
    [40]  начало 41-го периода т=20 мкс 
    [41]  начало 42-го периода т=20,5 мкс
    [42]  начало 43-го периода т=21 мкс
    [43]  начало 44-го периода т=21,5 мкс     
    [44]  начало 45-го периода т=22 мкс        ------------------ **** PK3 начало - ОК4 (-)

   

    Наиболее эффективный фильтр получается по принципу "n за t" - n-штук сообщений за контрольный интервал времени t.
    Если выполнилось двойное условие - считаем (hex или К1) достоверным.
    Нет - сбрасываем счетчик и начинаем цикл заново.
    В УВД, из-за отсутствия CRC и других средств - n требуется больше, чем в ModeS.
    Зато и по стандарту их до 150 шт. в секунду может излучаться против 20-30 шт. (каждого подтипа) в adsb.    
*/

void createOK1 (uint16_t *m, uint oklen, uint maxval, uint beforeafter) {
	uint i;

	oklen = oklen + beforeafter*2;
	for(i=0; i<oklen; i++) m[i]=0;

	m[0+beforeafter]= maxval;  m[1+beforeafter]= 100; m[2+beforeafter]= 100; //формируем импульс 1,5мкс PK1
	m[12+beforeafter]=maxval;  m[13+beforeafter]=100; m[14+beforeafter]=100; //шасси выпущено 			PK2
	m[28+beforeafter]=maxval;  m[29+beforeafter]=100; m[30+beforeafter]=100; //формируем импульс 1,5мкс PK3

    //OK1
    //t=8.5mks 110
    /*
    [29]  т=0,5 мкс       
    [30]  т=1 мкс       
    [31]  т=1,5 мкс               
    [32]  т=2 мкс       
    [33]  т=2,5 мкс       
    [34]  т=3 мкс   
    [35]  т=3,5 мкс       
    [36]  т=4 мкс       
    [37]  т=4,5 мкс
    [38]  т=5 мкс       
    [39]  т=5.5 мкс       
    [40]  т=6 мкс               
    [41]  т=6.5 мкс       
    [42]  т=7 мкс       
    [43]  т=7.5 мкс   
    [44]  т=8 мкс       

    ---РКИ1 =1
    -----POS1 =1 SUMM(45-52)>SUMM(53-60)
    [45]  т=14 мкс     t=0 mks
    [46]  т=14.5 мкс   t=0.5 mks       
    [47]  т=15 мкс     t=1 mks  
    [48]  т=15,5 мкс   t=1.5 mks  
    [49]  т=16 мкс     t=2 mks
    [50]  т=16,5 мкс   t=2.5 mks
    [51]  т=17 мкс     t=3 mks
    [52]  т=17,5 мкс   t=3.5 mks
    -----POS2 =0   
    [53]  т=18 мкс     t=4mks   t=0 mks
    [54]  т=18.5 мкс   t=4.5mks t=0.5 mks       
    [55]  т=19 мкс     t=5mks   t=1 mks  
    [56]  т=19,5 мкс   t=5.5mks t=1.5 mks  
    [57]  т=20 мкс     t=6mks   t=2 mks
    [58]  т=20,5 мкс   t=6.5mks t=2.5 mks
    [59]  т=21 мкс     t=7mks   t=3 mks
    [60]  т=21,5 мкс   t=7.5mks t=3.5 mks

    ---РКИ2 =1   
    -----POS1 =1 SUMM(61-68)>SUMM(69-76)
    [61]  т=14 мкс     t=0 mks
    [62]  т=14.5 мкс   t=0.5 mks       
    [63]  т=15 мкс     t=1 mks  
    [64]  т=15,5 мкс   t=1.5 mks  
    [65]  т=16 мкс     t=2 mks
    [66]  т=16,5 мкс   t=2.5 mks
    [67]  т=17 мкс     t=3 mks
    [68]  т=17,5 мкс   t=3.5 mks
    -----POS2 =0   
    [69]  т=18 мкс     t=4mks   t=0 mks
    [70]  т=18.5 мкс   t=4.5mks t=0.5 mks       
    [71]  т=19 мкс     t=5mks   t=1 mks  
    [72]  т=19,5 мкс   t=5.5mks t=1.5 mks  
    [73]  т=20 мкс     t=6mks   t=2 mks
    [74]  т=20,5 мкс   t=6.5mks t=2.5 mks
    [75]  т=21 мкс     t=7mks   t=3 mks
    [76]  т=21,5 мкс   t=7.5mks t=3.5 mks

    ---РКИ3 =0   
    -----POS1 =0 SUMM(77-84)<SUMM(85-92)
    [77]  т=14 мкс     t=0 mks
    [78]  т=14.5 мкс   t=0.5 mks       
    [79]  т=15 мкс     t=1 mks  
    [80]  т=15,5 мкс   t=1.5 mks  
    [81]  т=16 мкс     t=2 mks
    [82]  т=16,5 мкс   t=2.5 mks
    [83]  т=17 мкс     t=3 mks
    [84]  т=17,5 мкс   t=3.5 mks
    -----POS2 =1   
    [85]  т=18 мкс     t=4mks   t=0 mks
    [86]  т=18.5 мкс   t=4.5mks t=0.5 mks       
    [87]  т=19 мкс     t=5mks   t=1 mks  
    [88]  т=19,5 мкс   t=5.5mks t=1.5 mks  
    [89]  т=20 мкс     t=6mks   t=2 mks
    [90]  т=20,5 мкс   t=6.5mks t=2.5 mks
    [91]  т=21 мкс     t=7mks   t=3 mks
    [92]  т=21,5 мкс   t=7.5mks t=3.5 mks    

    ---РКИ1 =1
    -----POS1 =1 SUMM(45-52)>SUMM(53-60)
    ---РКИ2 =1   
    -----POS1 =1 SUMM(61-68)>SUMM(69-76)
    ---РКИ3 =0   
    -----POS1 =0 SUMM(77-84)<SUMM(85-92)

    */  

	for (i=45; i<48; i++)	m[i+beforeafter]=maxval; //1
	for (i=53; i<56; i++)	m[i+beforeafter]=100;	 //0 = 10

	for (i=61; i<64; i++)	m[i+beforeafter]=maxval; //1
	for (i=69; i<72; i++)	m[i+beforeafter]=100;	 //0 = 10

	for (i=77; i<80; i++)	m[i+beforeafter]=100; 	 //0
	for (i=85; i<88; i++)	m[i+beforeafter]=maxval; //1 = 01			
}



void createOK2 (uint16_t *m, uint oklen, uint maxval, uint beforeafter) {
	uint i;

	oklen = oklen + beforeafter*2;
	for(i=0; i<oklen; i++) m[i]=0;

	m[0+beforeafter]=maxval; m[1+beforeafter]=100; m[2+beforeafter]=100;	//формируем импульс 1,5мкс
	m[22+beforeafter]=maxval; m[23+beforeafter]=100; m[24+beforeafter]=100; //формируем импульс 1,5мкс
}



void createOK3 (uint16_t *m, uint oklen, uint maxval, uint beforeafter) {
	uint i;

	oklen = oklen + beforeafter*2;
	for(i=0; i<oklen; i++) m[i]=0;

	m[0+beforeafter]=maxval; m[1+beforeafter]=100; m[2+beforeafter]=100;	//формируем импульс 1,5мкс
	m[36+beforeafter]=maxval; m[37+beforeafter]=100; m[38+beforeafter]=100; //формируем импульс 1,5мкс
}