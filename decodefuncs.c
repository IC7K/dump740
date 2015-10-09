
uint decodePOS(uint16_t *m, uint32_t pkkoffs, uint32_t pkkpulselevel) {

return (uint32_t) (m[pkkoffs]+m[pkkoffs+1]+m[pkkoffs+2]+m[pkkoffs+3]+m[pkkoffs+4]+m[pkkoffs+5]+m[pkkoffs+6]+m[pkkoffs+7])/8 > pkkpulselevel ? 1 : 0; 

} //end decodePOS

// uint decodePOSPrint(uint16_t *m, uint32_t pkkoffs, uint32_t pkkpulselevel) {

// // printf("%d%d%d%d%d%d%d%d",m[pkkoffs],m[pkkoffs+1],m[pkkoffs+2],m[pkkoffs+3],m[pkkoffs+4],m[pkkoffs+5],m[pkkoffs+6],m[pkkoffs+7]);
// printf("%d", (uint32_t) (m[pkkoffs]+m[pkkoffs+1]+m[pkkoffs+2]+m[pkkoffs+3]+m[pkkoffs+4]+m[pkkoffs+5]+m[pkkoffs+6]+m[pkkoffs+7])/8 > pkkpulselevel ? 1 : 0);
// return (uint32_t) (m[pkkoffs]+m[pkkoffs+1]+m[pkkoffs+2]+m[pkkoffs+3]+m[pkkoffs+4]+m[pkkoffs+5]+m[pkkoffs+6]+m[pkkoffs+7])/8 > pkkpulselevel ? 1 : 0; 

// } //end decodePOS





// 6 - OK1, 0 - OK2, 5 - OK3
int decodeKEY(uint16_t *m, uint32_t pkkoffs, uint32_t pkkpulselevel) {
// uint32_t pkkmediana, pkkpulselevel, i, pkkend;    
uint p1, p2, p3, p4, p5, p6;
// uint result;

//определение среднего значения в ряде периодов для выделения посылки над помехами
// pkkmediana = 0;
// pkkend = pkkoffs + UVD_KEY_KODE_LEN;
// for (i = pkkoffs; i < pkkend; i++) { 
//     pkkmediana+=m[i]; //SUMM(ALL)
// }    
// pkkmediana = pkkmediana / UVD_KEY_KODE_LEN;     //48 периодов 0,5мкс в коде
// pkkpulselevel = pkkmediana / 2 + pkkmediana;

p1 = decodePOS(m, pkkoffs,    pkkpulselevel);
p2 = decodePOS(m, pkkoffs+8,  pkkpulselevel);
p3 = decodePOS(m, pkkoffs+16, pkkpulselevel);
p4 = decodePOS(m, pkkoffs+24, pkkpulselevel);
p5 = decodePOS(m, pkkoffs+32, pkkpulselevel);
p6 = decodePOS(m, pkkoffs+40, pkkpulselevel);

// 6 - OK1, 0 - OK2, 5 - OK3, -1 - ERROR

        if( //101 OK3
            p1>p2 &&    //10
            p3<p4 &&    //01
            p5>p6       //10
          ) return 5; else

        if( //000 OK2
            p1<p2 &&    //01
            p3<p4 &&    //01
            p5<p6       //01
          ) return 0; else

        if( //110 OK1
            p1>p2 &&    //10
            p3>p4 &&    //10
            p5<p6       //01
          ) return 6; else

        return -1;

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


if(b1==b2 || b3==b4 || b5==b6 || b7==b8)
    {
        //ошибка в кодировании 1 или 0 - должно быть или 10 или 01, но не 11 или 00
        result = -1;
    }
else
    {
        result = 0;
        result = ((b1 < b2) ? 0 : 1) | ((b3 < b4) ? 0 : 2) | ((b5 < b6) ? 0 : 4) | ((b7 < b8) ? 0 : 8);
    }

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

if(b1==b2 || b3==b4 || b5==b6 || b7==b8)
    {
        //ошибка в кодировании 1 или 0 - должно быть или 10 или 01, но не 11 или 00
        result = -1;
    }
else
    {
        result = 0;
        result = ((b1 < b2) ? 0 : 8) | ((b3 < b4) ? 0 : 4) | ((b5 < b6) ? 0 : 2) | ((b7 < b8) ? 0 : 1);
    }

return result; 
} //end decodeDECADEFUEL