    //******************* OK1 **********************

    if((uint32_t) ((m[j+27]+m[j+28]+m[j+29])/3)>pulselevel) {       

    oscilloscope(m, 0,  UVD_KOORD_KODE_LEN, pulselevel);
    createOK1 (ok1koord, UVD_KOORD_KODE_LEN);
    oscilloscope(ok1koord, 0,  UVD_KOORD_KODE_LEN, pulselevel); 
    
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
        //определение среднего значения в ряде периодов для выделения посылки над помехами
        pkkoffs = UVD_OK1_OFFS; //45
        pkkpulselevel = pulselevel;

        // pkkmediana = 0;
        // pkkend = pkkoffs + UVD_KEY_KODE_LEN; //+48=93
        // for (i = pkkoffs; i < pkkend; i++) { 
        //     pkkmediana+=m[j+i]; //SUMM(ALL)
        // }    
        // pkkmediana = pkkmediana / UVD_KEY_KODE_LEN;     //48 периодов 0,5мкс в коде
        // pkkpulselevel = pkkmediana / 2 + pkkmediana;

        //декодирование ключевого кода
        okval = decodeKEY(m, j+pkkoffs, pkkpulselevel); // 6 - OK1, 0 - OK2, 5 - OK3, -1 - ERROR

        if(okval==6)
        {
        //DECODE INFO CODE - BORT NUMBER  RF- D5 D4 D3 D2 D1

        //START DECADE 1 - bits for positions
        //8*8=64 periods of 0.5mks
        pkkoffs = pkkoffs + UVD_KEY_KODE_LEN;
        dec1 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        // if(dec1 == (int) '*') continue; //11 или 00 вместо 10 или 01
        //END DECADE 1



        //START DECADE 2 - bits for positions
        //8*8=64 periods of 0.5mks
        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec2 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        // if(dec2 == (int) '*') continue; //11 или 00 вместо 10 или 01
        //END DECADE 2



        //START DECADE 3 - bits for positions
        //8*8=64 periods of 0.5mks
        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks
        dec3 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        // if(dec3 == (int) '*') continue; //11 или 00 вместо 10 или 01
        //END DECADE 3



        //START DECADE 4 - bits for positions
        //8*8=64 periods of 0.5mks
        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec4 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        // if(dec4 == (int) '*') continue; //11 или 00 вместо 10 или 01
        //END DECADE 4



        //START DECADE 5 - bits for positions
        //8*8=64 periods of 0.5mks
        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec5 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        // if(dec5 == (int) '*') continue; //11 или 00 вместо 10 или 01
        //END DECADE 5


        //REPEAT DECADES
        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec1r = decodeDECADE(m, pkkoffs, pkkpulselevel);
        // if(dec1r==-1) dec1r = (int) '*'; else //continue; //11 или 00 вместо 10 или 01
        // if(dec1r!=dec1) continue; 
        //END DECADE 1

        //START DECADE 2 - bits for positions
        //8*8=64 periods of 0.5mks
        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec2r = decodeDECADE(m, pkkoffs, pkkpulselevel);
        // if(dec2r==-1) dec2r = (int) '*'; else // continue; //11 или 00 вместо 10 или 01
        // if(dec2r!=dec2) continue;
        //END DECADE 2

        //START DECADE 3 - bits for positions
        //8*8=64 periods of 0.5mks
        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec3r = decodeDECADE(m, pkkoffs, pkkpulselevel);
        // if(dec3r==-1) dec3r = (int) '*'; else // continue; //11 или 00 вместо 10 или 01
        // if(dec3r!=dec3) continue;
        //END DECADE 3

        //START DECADE 4 - bits for positions
        //8*8=64 periods of 0.5mks
        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec4r = decodeDECADE(m, pkkoffs, pkkpulselevel);
        // if(dec4r==-1) dec4r = (int) '*'; else // continue; //11 или 00 вместо 10 или 01
        // if(dec4r!=dec4) continue;
        //END DECADE 4

        //START DECADE 5 - bits for positions
        //8*8=64 periods of 0.5mks
        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec5r = decodeDECADE(m, pkkoffs, pkkpulselevel);
        // if(dec5r==-1) dec5r = (int) '*'; else // continue; //11 или 00 вместо 10 или 01
        // if(dec5r!=dec5) continue;
        //END DECADE 5



        //DECODE INFO CODE - BORT NUMBER  RF- D5 D4 D3 D2 D1

            // printf("%s - OK1 OK RKK=110 [%d>%d - %d>%d - %d<%d] %d>%d MED=%d\n",
            //     timestr,
            //     m[j+pkkoffs]+m[j+pkkoffs+1]+m[j+pkkoffs+2]+m[j+pkkoffs+3]+m[j+pkkoffs+4]+m[j+pkkoffs+5]+m[j+pkkoffs+6]+m[j+pkkoffs+7],
            //     m[j+pkkoffs+8]+m[j+pkkoffs+9]+m[j+pkkoffs+10]+m[j+pkkoffs+11]+m[j+pkkoffs+12]+m[j+pkkoffs+13]+m[j+pkkoffs+14]+m[j+pkkoffs+15],
            //     m[j+pkkoffs+16]+m[j+pkkoffs+17]+m[j+pkkoffs+18]+m[j+pkkoffs+19]+m[j+pkkoffs+20]+m[j+pkkoffs+21]+m[j+pkkoffs+22]+m[j+pkkoffs+23],
            //     m[j+pkkoffs+24]+m[j+pkkoffs+25]+m[j+pkkoffs+26]+m[j+pkkoffs+27]+m[j+pkkoffs+28]+m[j+pkkoffs+29]+m[j+pkkoffs+30]+m[j+pkkoffs+31],
            //     m[j+pkkoffs+32]+m[j+pkkoffs+33]+m[j+pkkoffs+34]+m[j+pkkoffs+35]+m[j+pkkoffs+36]+m[j+pkkoffs+37]+m[j+pkkoffs+38]+m[j+pkkoffs+39],
            //     m[j+pkkoffs+40]+m[j+pkkoffs+41]+m[j+pkkoffs+42]+m[j+pkkoffs+43]+m[j+pkkoffs+44]+m[j+pkkoffs+45]+m[j+pkkoffs+46]+m[j+pkkoffs+47],
            //     m[j],
            //     pulselevel,
            //     mediana
            //     );
            printf("\n%s - OK1 OK RKK=110 6 REGN=%c%c%c%c%c\n", timestr, (char) dec5, (char) dec4, (char) dec3, (char) dec2, (char) dec1);
            printf("%s - OK1rOK RKK=110 6 REGN=%c%c%c%c%c\n", timestr, (char) dec5r, (char) dec4r, (char) dec3r, (char) dec2r, (char) dec1r);

            // for(i=0;i<UVD_MAX_LEN;i++) {
            // marrwrite[i] = m[j+i];
            // }

            // sprintf(filestr, "ok1-%02d-%02d-%02d-%03d.data", (int) t->tm_hour, (int) t->tm_min, (int) t->tm_sec, (int) tp.tv_usec/1000);
            // FILE *f = fopen(filestr, "wb");
            // fwrite(marrwrite, sizeof(uint16_t), sizeof(marrwrite), f);
            // fclose(f);


            j+=UVD_KOORD_KODE_LEN+UVD_OK1_DELAY+UVD_KEY_KODE_LEN+UVD_INFO_KODE_LEN;
            continue;
        } 
            // else printf("OK1 BAD RKK\n");
    } //end OK1