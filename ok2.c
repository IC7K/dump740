    //******************* OK2 **********************

    if((uint32_t) ((m[j+21]+m[j+22]+m[j+23])/3)>pulselevel) {
    //OK2
    //t=14mks 000
    /*
    [23]  т=0,5 мкс       
    [24]  т=1 мкс       
    [25]  т=1,5 мкс               
    [26]  т=2 мкс       
    [27]  т=2,5 мкс       
    [28]  т=3 мкс   
    [29]  т=3,5 мкс       
    [30]  т=4 мкс       
    [31]  т=4,5 мкс
    [32]  т=5 мкс       
    [33]  т=5.5 мкс       
    [34]  т=6 мкс               
    [35]  т=6.5 мкс       
    [36]  т=7 мкс       
    [37]  т=7.5 мкс   
    [38]  т=8 мкс       
    [39]  т=8.5 мкс       
    [40]  т=9 мкс          
    [41]  т=9.5 мкс               
    [42]  т=10 мкс       
    [43]  т=10.5 мкс       
    [44]  т=11 мкс   
    [45]  т=11.5 мкс       
    [46]  т=12 мкс       
    [47]  т=12.5 мкс     
    [48]  т=13 мкс               
    [49]  т=13.5 мкс   

    ---РКИ1 =0
    -----POS1 =0 SUMM(50-57)<SUMM(58-65)
    [50]  т=14 мкс     t=0 mks
    [51]  т=14.5 мкс   t=0.5 mks       
    [52]  т=15 мкс     t=1 mks  
    [53]  т=15,5 мкс   t=1.5 mks  
    [54]  т=16 мкс     t=2 mks
    [55]  т=16,5 мкс   t=2.5 mks
    [56]  т=17 мкс     t=3 mks
    [57]  т=17,5 мкс   t=3.5 mks
    -----POS2 =1   
    [58]  т=18 мкс     t=4mks   t=0 mks
    [59]  т=18.5 мкс   t=4.5mks t=0.5 mks       
    [60]  т=19 мкс     t=5mks   t=1 mks  
    [61]  т=19,5 мкс   t=5.5mks t=1.5 mks  
    [62]  т=20 мкс     t=6mks   t=2 mks
    [63]  т=20,5 мкс   t=6.5mks t=2.5 mks
    [64]  т=21 мкс     t=7mks   t=3 mks
    [65]  т=21,5 мкс   t=7.5mks t=3.5 mks

    ---РКИ2 =0   
    -----POS1 =0 SUMM(66-73)<SUMM(74-81)
    [66]  т=14 мкс     t=0 mks
    [67]  т=14.5 мкс   t=0.5 mks       
    [68]  т=15 мкс     t=1 mks  
    [69]  т=15,5 мкс   t=1.5 mks  
    [70]  т=16 мкс     t=2 mks
    [71]  т=16,5 мкс   t=2.5 mks
    [72]  т=17 мкс     t=3 mks
    [73]  т=17,5 мкс   t=3.5 mks
    -----POS2 =1   
    [74]  т=18 мкс     t=4mks   t=0 mks
    [75]  т=18.5 мкс   t=4.5mks t=0.5 mks       
    [76]  т=19 мкс     t=5mks   t=1 mks  
    [77]  т=19,5 мкс   t=5.5mks t=1.5 mks  
    [78]  т=20 мкс     t=6mks   t=2 mks
    [79]  т=20,5 мкс   t=6.5mks t=2.5 mks
    [80]  т=21 мкс     t=7mks   t=3 mks
    [81]  т=21,5 мкс   t=7.5mks t=3.5 mks

    ---РКИ3 =0   
    -----POS1 =0 SUMM(82-89)<SUMM(90-97)
    [82]  т=14 мкс     t=0 mks
    [83]  т=14.5 мкс   t=0.5 mks       
    [84]  т=15 мкс     t=1 mks  
    [85]  т=15,5 мкс   t=1.5 mks  
    [86]  т=16 мкс     t=2 mks
    [87]  т=16,5 мкс   t=2.5 mks
    [88]  т=17 мкс     t=3 mks
    [89]  т=17,5 мкс   t=3.5 mks
    -----POS2 =1   
    [90]  т=18 мкс     t=4mks   t=0 mks
    [91]  т=18.5 мкс   t=4.5mks t=0.5 mks       
    [92]  т=19 мкс     t=5mks   t=1 mks  
    [93]  т=19,5 мкс   t=5.5mks t=1.5 mks  
    [94]  т=20 мкс     t=6mks   t=2 mks
    [95]  т=20,5 мкс   t=6.5mks t=2.5 mks
    [96]  т=21 мкс     t=7mks   t=3 mks
    [97]  т=21,5 мкс   t=7.5mks t=3.5 mks    

    ---РКИ1 =0
    -----POS1 =0 SUMM(50-57)<SUMM(58-65)  
    ---РКИ2 =0   
    -----POS1 =0 SUMM(66-73)<SUMM(74-81)
    ---РКИ3 =0   
    -----POS1 =0 SUMM(82-89)<SUMM(90-97)                
    */      



        //определение среднего значения в ряде периодов для выделения посылки над помехами
        pkkoffs = UVD_OK2_OFFS; //50
        // pkkpulselevel = pulselevel;
        
        pkkmediana = 0;
        pkkend = pkkoffs + UVD_KEY_KODE_LEN; //+48=97
        for (i = pkkoffs; i < pkkend; i++) { 
            pkkmediana+=m[j+i]; //SUMM(ALL)
        }    
        pkkmediana = pkkmediana / UVD_KEY_KODE_LEN;     //48 периодов 0,5мкс в коде
        pkkpulselevel = pkkmediana / 2 + pkkmediana;

        //декодирование ключевого кода
        okval = decodeKEY(m, j+pkkoffs, pkkpulselevel); // 6 - OK1, 0 - OK2, 5 - OK3, -1 - ERROR

        if(okval == 0)
        {

        pkkoffs = pkkoffs + UVD_KEY_KODE_LEN;
        pkkpulselevel = pulselevel;

        dec1 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        // if(dec1==-1) continue; //11 или 00 вместо 10 или 01
        dec1+=(int) '0';

        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec2 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        // if(dec2==-1) continue; //11 или 00 вместо 10 или 01
        dec2+=(int) '0';

        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec3 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        // if(dec3==-1) continue; //11 или 00 вместо 10 или 01
        dec3+=(int) '0';

        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec4 = decodeDECADE(m, pkkoffs, pkkpulselevel);
        // if(dec4==-1) continue; //11 или 00 вместо 10 или 01
        dec4+=(int) '0';

        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec5 = decodeDECADEFUEL(m, pkkoffs, pkkpulselevel);
        // if(dec5==-1) continue; //11 или 00 вместо 10 или 01

        int fuel = (dec5<10) ? dec5*5 : (dec5-5)*10;

        //REPEAT DECADES
        // следущее - это повторение
        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec1r = decodeDECADE(m, pkkoffs, pkkpulselevel);
        // if(dec1r==-1) continue; //11 или 00 вместо 10 или 01
        dec1r+=(int) '0';
        // if(dec1r!=dec1) continue; 

        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec2r = decodeDECADE(m, pkkoffs, pkkpulselevel);
        // if(dec2r==-1) continue; //11 или 00 вместо 10 или 01
        dec2r+=(int) '0';
        // if(dec2r!=dec2) continue; 

        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec3r = decodeDECADE(m, pkkoffs, pkkpulselevel);
        // if(dec3r==-1) continue; //11 или 00 вместо 10 или 01
        dec3r+=(int) '0';
        // if(dec3r!=dec3) continue; 

        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec4r = decodeDECADE(m, pkkoffs, pkkpulselevel);
        // if(dec4r==-1) continue; //11 или 00 вместо 10 или 01
        dec4r+=(int) '0';
        // if(dec4r!=dec4) continue; 

        pkkoffs = pkkoffs + UVD_DECADE_LEN; //+64 periods by 0.5mks

        dec5r = decodeDECADEFUEL(m, pkkoffs, pkkpulselevel);
        // if(dec5r==-1) continue; //11 или 00 вместо 10 или 01
        // if(dec5r!=dec5) continue; 

        int fuelr = (dec5r<10) ? dec5r*5 : (dec5r-5)*10;

        //Print result
            // printf("%s - OK2 OK RKK=000 [%d<%d - %d<%d - %d<%d] %d>%d MED=%d OK2VAL(0)=%d\n",
            //     timestr,
            //     m[j+pkkoffs]+m[j+pkkoffs+1]+m[j+pkkoffs+2]+m[j+pkkoffs+3]+m[j+pkkoffs+4]+m[j+pkkoffs+5]+m[j+pkkoffs+6]+m[j+pkkoffs+7],
            //     m[j+pkkoffs+8]+m[j+pkkoffs+9]+m[j+pkkoffs+10]+m[j+pkkoffs+11]+m[j+pkkoffs+12]+m[j+pkkoffs+13]+m[j+pkkoffs+14]+m[j+pkkoffs+15],
            //     m[j+pkkoffs+16]+m[j+pkkoffs+17]+m[j+pkkoffs+18]+m[j+pkkoffs+19]+m[j+pkkoffs+20]+m[j+pkkoffs+21]+m[j+pkkoffs+22]+m[j+pkkoffs+23],
            //     m[j+pkkoffs+24]+m[j+pkkoffs+25]+m[j+pkkoffs+26]+m[j+pkkoffs+27]+m[j+pkkoffs+28]+m[j+pkkoffs+29]+m[j+pkkoffs+30]+m[j+pkkoffs+31],
            //     m[j+pkkoffs+32]+m[j+pkkoffs+33]+m[j+pkkoffs+34]+m[j+pkkoffs+35]+m[j+pkkoffs+36]+m[j+pkkoffs+37]+m[j+pkkoffs+38]+m[j+pkkoffs+39],
            //     m[j+pkkoffs+40]+m[j+pkkoffs+41]+m[j+pkkoffs+42]+m[j+pkkoffs+43]+m[j+pkkoffs+44]+m[j+pkkoffs+45]+m[j+pkkoffs+46]+m[j+pkkoffs+47],
            //     m[j],
            //     pulselevel,
            //     mediana,
            //     // 6 - OK1, 0 - OK2, 5 - OK3
            //     okval
            //     );

        printf("\n%s - OK2 OK RKK=000 0 ALT=%c%c%c%c0m  FUEL=%d%%\n", timestr, (char) (0b1100 & dec4)>>2, (char) dec3, (char) dec2, (char) dec1, fuel);            
        printf("\n%s - OK2rOK RKK=000 0 ALT=%c%c%c%c0m  FUEL=%d%%\n", timestr, (char) (0b1100 & dec4r)>>2, (char) dec3r, (char) dec2r, (char) dec1r, fuelr); 
            // for(i=0;i<UVD_MAX_LEN;i++) {
            // marrwrite[i] = m[j+i];
            // }

            // sprintf(filestr, "ok2-%02d-%02d-%02d-%03d.data", (int) t->tm_hour, (int) t->tm_min, (int) t->tm_sec, (int) tp.tv_usec/1000);
            // FILE *f = fopen(filestr, "wb");
            // fwrite(marrwrite, sizeof(uint16_t), sizeof(marrwrite), f);
            // fclose(f);


            j+=UVD_KOORD_KODE_LEN+UVD_OK2_DELAY+UVD_KEY_KODE_LEN+UVD_INFO_KODE_LEN;
            continue;
        } 
            // else printf("%s - OK2 BAD RKK\n", timestr);
    } //end OK2
      

    // continue;