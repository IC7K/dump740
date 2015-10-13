uint decodePOS(uint16_t *m, uint32_t pkkoffs, uint32_t pkkpulselevel) {

return (uint32_t) ((m[pkkoffs]+m[pkkoffs+1]+m[pkkoffs+2]+m[pkkoffs+3]+m[pkkoffs+4]+m[pkkoffs+5]+m[pkkoffs+6]+m[pkkoffs+7])/8) > pkkpulselevel ? 1 : 0; 

} //end decodePOS

uint decodeZERO8(uint16_t *m, uint32_t pkkoffs, uint32_t pkkpulselevel) {

return (uint32_t) ((m[pkkoffs]+m[pkkoffs+1]+m[pkkoffs+2]+m[pkkoffs+3]+m[pkkoffs+4]+m[pkkoffs+5]+m[pkkoffs+6]+m[pkkoffs+7])/8) < pkkpulselevel ? 1 : 0; 

} //end decodeZERO


int decodePKI16(uint16_t *m, uint32_t pkkoffs, uint32_t pkkpulselevel) {
// uint16_t maxlevel;

//детектируем 1 в первом разряде
//считаем за 1 превышение в первые 1,5мкс уровня порога
// if(m[pkkoffs]>pkkpulselevel || m[pkkoffs+1]>pkkpulselevel || m[pkkoffs+2]>pkkpulselevel) //слишком много срабатываний
    //видел импульс и 1 мкс!!!!!!!!!!
if( (uint32_t )((m[pkkoffs]+m[pkkoffs+1])/2)>pkkpulselevel)    
    {
    //считаем код за 1
    // maxlevel = 0;
    // if(maxlevel<m[pkkoffs])   maxlevel = m[pkkoffs];
    // if(maxlevel<m[pkkoffs+1]) maxlevel = m[pkkoffs+1];
    // if(maxlevel<m[pkkoffs+2]) maxlevel = m[pkkoffs+2];

        //проверяем 0 во втором разряде, иначе ошибка!
        //0 код будет тогда, когда среднее за 8 периодов меньше макс уровня единицы
        if(decodeZERO8(m, pkkoffs+8, pkkpulselevel) == 1)
        {
            //возвращаем корректный код 10 = 1
            return 1; // 0b10
        }
        // else return -1;

    } //end 1 in first pos

//проверяем тогда 1 во втором разряде, иначе ошибка!
//считаем за 1 превышение в первые 1,5мкс уровня порога  

// if(m[pkkoffs+8]>pkkpulselevel || m[pkkoffs+9]>pkkpulselevel || m[pkkoffs+10]>pkkpulselevel) //слишком много срабатываний
    //видел импульс и 1 мкс!!!!!!!!!!
if((uint32_t )((m[pkkoffs+8]+m[pkkoffs+9])/2)>pkkpulselevel)       
   {
    //считаем код за 1
    // maxlevel = 0;
    // if(maxlevel<m[pkkoffs+8])  maxlevel = m[pkkoffs+8];
    // if(maxlevel<m[pkkoffs+9])  maxlevel = m[pkkoffs+9];
    // if(maxlevel<m[pkkoffs+10]) maxlevel = m[pkkoffs+10];

    //проверяем тогда 0 в первом разряде, иначе ошибка!
        if(decodeZERO8(m, pkkoffs, pkkpulselevel) == 1)
        {

            //возвращаем корректный код 01 = 0
            return 0; // 0b01
        }
        // else return -1;

   }

//ни один вариант не сработал - ОШИБКА!
return -1;

} //end decodePKI







// uint32_t decodePOSRAW(uint16_t *m, uint32_t pkkoffs) {

// return (uint32_t) (m[pkkoffs]+m[pkkoffs+1]+m[pkkoffs+2]+m[pkkoffs+3]+m[pkkoffs+4]+m[pkkoffs+5]+m[pkkoffs+6]+m[pkkoffs+7]); 

// } //end decodePOS
// uint decodePOSPrint(uint16_t *m, uint32_t pkkoffs, uint32_t pkkpulselevel) {

// // printf("%d%d%d%d%d%d%d%d",m[pkkoffs],m[pkkoffs+1],m[pkkoffs+2],m[pkkoffs+3],m[pkkoffs+4],m[pkkoffs+5],m[pkkoffs+6],m[pkkoffs+7]);
// printf("%d", (uint32_t) (m[pkkoffs]+m[pkkoffs+1]+m[pkkoffs+2]+m[pkkoffs+3]+m[pkkoffs+4]+m[pkkoffs+5]+m[pkkoffs+6]+m[pkkoffs+7])/8 > pkkpulselevel ? 1 : 0);
// return (uint32_t) (m[pkkoffs]+m[pkkoffs+1]+m[pkkoffs+2]+m[pkkoffs+3]+m[pkkoffs+4]+m[pkkoffs+5]+m[pkkoffs+6]+m[pkkoffs+7])/8 > pkkpulselevel ? 1 : 0; 

// } //end decodePOS





// 1(6) - OK1, 2(0) - OK2, 3(5) - OK3
int decodeKEY(uint16_t *m, uint32_t pkkoffs, uint32_t pkkpulselevel) {
// uint32_t pkkmediana, pkkpulselevel, i, pkkend;    
// uint32_t p1, p2, p3, p4, p5, p6;
int b1, b2, b3;
// uint result;


b1 = decodePKI16(m, pkkoffs,    pkkpulselevel);
b2 = decodePKI16(m, pkkoffs+16, pkkpulselevel);
b3 = decodePKI16(m, pkkoffs+32, pkkpulselevel);

// p1 = decodePOS(m, pkkoffs,    pkkpulselevel);
// p2 = decodePOS(m, pkkoffs+8,  pkkpulselevel);
// p3 = decodePOS(m, pkkoffs+16, pkkpulselevel);
// p4 = decodePOS(m, pkkoffs+24, pkkpulselevel);
// p5 = decodePOS(m, pkkoffs+32, pkkpulselevel);
// p6 = decodePOS(m, pkkoffs+40, pkkpulselevel);

if((b1 == 1) && (b2 == 0) & (b3 == 1)) return 3; else
if((b1 == 0) && (b2 == 0) & (b3 == 0)) return 2; else
if((b1 == 1) && (b2 == 1) & (b3 == 0)) return 1; else
return -1;


// 6 - OK1, 0 - OK2, 5 - OK3, -1 - ERROR

        // if( //101 OK3
        //     p1>p2 &&    //10
        //     p3<p4 &&    //01
        //     p5>p6       //10
        //   ) return 5; else

        // if( //000 OK2
        //     p1<p2 &&    //01
        //     p3<p4 &&    //01
        //     p5<p6       //01
        //   ) return 0; else

        // if( //110 OK1
        //     p1>p2 &&    //10
        //     p3>p4 &&    //10
        //     p5<p6       //01
        //   ) return 6; else

        // //ДРУГИХ КОМБИНАЦИЙ НЕТ (ЗК4 на будущее)
        // return -1;

// if(p1==p2 || p3==p4 || p5==p6)
//     {
//         //ошибка в кодировании 1 или 0 - должно быть или 10 или 01, но не 11 или 00
//         result = -1;
//     }
// else
//     {
//         result = 0;
//         result = ((p1 < p2) ? 0 : 4) | ((p3 < p4) ? 0 : 2) | ((p5 < p6) ? 0 : 1);
//     }
// return result; 

} //end decodeKEY




int decodeDECADE(uint16_t *m, uint32_t pkkoffs, uint32_t pkkpulselevel) {
// uint32_t pkkmediana, pkkpulselevel, i, pkkend;
int result, b1, b2, b3, b4;//, b5, b6, b7, b8;


b1 = decodePKI16(m, pkkoffs,    pkkpulselevel);
b2 = decodePKI16(m, pkkoffs+16, pkkpulselevel);
b3 = decodePKI16(m, pkkoffs+32, pkkpulselevel);
b4 = decodePKI16(m, pkkoffs+48, pkkpulselevel);

// b1 = decodePOS(m, pkkoffs,    pkkpulselevel);
// b2 = decodePOS(m, pkkoffs+8,  pkkpulselevel);
// // printf(" ");
// b3 = decodePOS(m, pkkoffs+16, pkkpulselevel);
// b4 = decodePOS(m, pkkoffs+24, pkkpulselevel);
// // printf(" ");
// b5 = decodePOS(m, pkkoffs+32, pkkpulselevel);
// b6 = decodePOS(m, pkkoffs+40, pkkpulselevel);
// // printf(" ");
// b7 = decodePOS(m, pkkoffs+48, pkkpulselevel);
// b8 = decodePOS(m, pkkoffs+56, pkkpulselevel);
// printf("  END DECADE\n");


result = 0;

if(b1==1) result|=1;
if(b2==1) result|=2;
if(b3==1) result|=4;
if(b4==1) result|=8;

if(b1==-1 || b2==-1 || b3==-1 || b4==-1)
    {
        //ошибка в кодировании 1 или 0 - должно быть или 10 или 01, но не 11 или 00
        result = (int) '*';
    }
    //ошибки нет
    else    result+=(int) '0';    
// else
//     {
//         result = 0;
//         result = ((b1 < b2) ? 0 : 1) | ((b3 < b4) ? 0 : 2) | ((b5 < b6) ? 0 : 4) | ((b7 < b8) ? 0 : 8);
//     }

return result; 
} //end decodeDECADE







int decodeDECADEFUEL(uint16_t *m, uint32_t pkkoffs, uint32_t pkkpulselevel) {
// uint32_t pkkmediana, pkkpulselevel, i, pkkend;
uint result, b1, b2, b3, b4, b5, b6, b7, b8;

//определение среднего значения в ряде периодов для выделения посылки над помехами
// pkkmediana = 0;
// pkkend = pkkoffs + UVD_DECADE_LEN;
// for (i = pkkoffs; i < pkkend; i++) { 
//     pkkmediana+=m[i]; //SUMM(ALL)
// }    
// pkkmediana = pkkmediana / UVD_DECADE_LEN;     
// pkkpulselevel = pkkmediana / 2 + pkkmediana;

// printf("START DECADE ");
b1 = decodePOS(m, pkkoffs,    pkkpulselevel);
b2 = decodePOS(m, pkkoffs+8,  pkkpulselevel);
// printf(" ");
b3 = decodePOS(m, pkkoffs+16, pkkpulselevel);
b4 = decodePOS(m, pkkoffs+24, pkkpulselevel);
// printf(" ");
b5 = decodePOS(m, pkkoffs+32, pkkpulselevel);
b6 = decodePOS(m, pkkoffs+40, pkkpulselevel);
// printf(" ");
b7 = decodePOS(m, pkkoffs+48, pkkpulselevel);
b8 = decodePOS(m, pkkoffs+56, pkkpulselevel);
// printf("  END DECADE\n");

//если по уровням пришло оба 11 или 00 то сравниваем тупо сумму значений периодов без учета уровня 1
// if (b1==b2) { b1=decodePOSRAW(m,pkkoffs);    b2=decodePOSRAW(m,pkkoffs+ 8); }
// if (b3==b4) { b3=decodePOSRAW(m,pkkoffs+16); b4=decodePOSRAW(m,pkkoffs+24); }
// if (b5==b6) { b5=decodePOSRAW(m,pkkoffs+32); b6=decodePOSRAW(m,pkkoffs+40); }
// if (b7==b8) { b7=decodePOSRAW(m,pkkoffs+48); b8=decodePOSRAW(m,pkkoffs+56); }

result = 0;

//порядок разрядов наоборот для FUEL
if(b1>b2) result|=8;
if(b3>b4) result|=4;
if(b5>b6) result|=2;
if(b7>b8) result|=1;

if(b1==b2 || b3==b4 || b5==b6 || b7==b8)
    {
        //ошибка в кодировании 1 или 0 - должно быть или 10 или 01, но не 11 или 00
        result = -1;// (int) '*';
    }
    //ошибки нет
    // else    result+=(int) '0';
// else
//     {
//         result = 0;
//         result = ((b1 < b2) ? 0 : 8) | ((b3 < b4) ? 0 : 4) | ((b5 < b6) ? 0 : 2) | ((b7 < b8) ? 0 : 1);
//     }

return result; 
} //end decodeDECADEFUEL