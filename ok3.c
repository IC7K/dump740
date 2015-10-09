    //******************* OK3 **********************  

    if((uint32_t) ((m[j+35]+m[j+36]+m[j+37])/3)>pulselevel) {         
    //OK3
    //t=10mks 101
    /*
    [37]  т=0,5 мкс       
    [38]  т=1 мкс       
    [39]  т=1,5 мкс               
    [40]  т=2 мкс       
    [41]  т=2,5 мкс       
    [42]  т=3 мкс   
    [43]  т=3,5 мкс       
    [44]  т=4 мкс       
    [45]  т=4,5 мкс
    [46]  т=5 мкс       
    [47]  т=5.5 мкс       
    [48]  т=6 мкс               
    [49]  т=6.5 мкс       
    [50]  т=7 мкс       
    [51]  т=7.5 мкс   
    [52]  т=8 мкс       
    [53]  т=8.5 мкс       
    [54]  т=9 мкс          
    [55]  т=9.5 мкс               


    ---РКИ1 =1
    -----POS1 =1 SUMM(56-63)>SUMM(64-71)
    [56]  т=14 мкс     t=0 mks
    [57]  т=14.5 мкс   t=0.5 mks       
    [58]  т=15 мкс     t=1 mks  
    [59]  т=15,5 мкс   t=1.5 mks  
    [60]  т=16 мкс     t=2 mks
    [61]  т=16,5 мкс   t=2.5 mks
    [62]  т=17 мкс     t=3 mks
    [63]  т=17,5 мкс   t=3.5 mks
    -----POS2 =0   
    [64]  т=18 мкс     t=4mks   t=0 mks
    [65]  т=18.5 мкс   t=4.5mks t=0.5 mks       
    [66]  т=19 мкс     t=5mks   t=1 mks  
    [67]  т=19,5 мкс   t=5.5mks t=1.5 mks  
    [68]  т=20 мкс     t=6mks   t=2 mks
    [69]  т=20,5 мкс   t=6.5mks t=2.5 mks
    [70]  т=21 мкс     t=7mks   t=3 mks
    [71]  т=21,5 мкс   t=7.5mks t=3.5 mks

    ---РКИ2 =0   
    -----POS1 =0 SUMM(72-79)<SUMM(80-87)
    [72]  т=14 мкс     t=0 mks
    [73]  т=14.5 мкс   t=0.5 mks       
    [74]  т=15 мкс     t=1 mks  
    [75]  т=15,5 мкс   t=1.5 mks  
    [76]  т=16 мкс     t=2 mks
    [77]  т=16,5 мкс   t=2.5 mks
    [78]  т=17 мкс     t=3 mks
    [79]  т=17,5 мкс   t=3.5 mks
    -----POS2 =1   
    [80]  т=18 мкс     t=4mks   t=0 mks
    [81]  т=18.5 мкс   t=4.5mks t=0.5 mks       
    [82]  т=19 мкс     t=5mks   t=1 mks  
    [83]  т=19,5 мкс   t=5.5mks t=1.5 mks  
    [84]  т=20 мкс     t=6mks   t=2 mks
    [85]  т=20,5 мкс   t=6.5mks t=2.5 mks
    [86]  т=21 мкс     t=7mks   t=3 mks
    [87]  т=21,5 мкс   t=7.5mks t=3.5 mks

    ---РКИ3 =1   
    -----POS1 =1 SUMM(88-95)>SUMM(96-103)   
    [88]  т=14 мкс     t=0 mks
    [89]  т=14.5 мкс   t=0.5 mks       
    [90]  т=15 мкс     t=1 mks  
    [91]  т=15,5 мкс   t=1.5 mks  
    [92]  т=16 мкс     t=2 mks
    [93]  т=16,5 мкс   t=2.5 mks
    [94]  т=17 мкс     t=3 mks
    [95]  т=17,5 мкс   t=3.5 mks
    -----POS2 =0
    [96]  т=18 мкс     t=4mks   t=0 mks
    [97]  т=18.5 мкс   t=4.5mks t=0.5 mks       
    [98]  т=19 мкс     t=5mks   t=1 mks  
    [99]  т=19,5 мкс   t=5.5mks t=1.5 mks  
    100]  т=20 мкс     t=6mks   t=2 mks
    101]  т=20,5 мкс   t=6.5mks t=2.5 mks
    102]  т=21 мкс     t=7mks   t=3 mks
    103]  т=21,5 мкс   t=7.5mks t=3.5 mks    

    ---РКИ1 =1
    -----POS1 =1 SUMM(56-63)>SUMM(64-71)
    ---РКИ2 =0   
    -----POS1 =0 SUMM(72-79)<SUMM(80-87)
    ---РКИ3 =1   
    -----POS1 =1 SUMM(88-95)>SUMM(96-103)                      
    */
                    //определение среднего значения в ряде периодов для выделения посылки над помехами
        // pkkmediana = 0;
        pkkoffs = UVD_OK3_OFFS; //56
        // pkkend = pkkoffs + UVD_KEY_KODE_LEN; //+48=104
        // for (i = pkkoffs; i < pkkend; i++) { 
        //     pkkmediana+=m[j+i]; //SUMM(ALL)
        // }    
        // pkkmediana = pkkmediana / UVD_KEY_KODE_LEN;     //48 периодов 0,5мкс в коде
        // pkkpulselevel = pkkmediana / 2 + pkkmediana;
        pkkpulselevel = pulselevel;

        p1 = (uint32_t) (m[j+pkkoffs]+m[j+pkkoffs+1]+m[j+pkkoffs+2]+m[j+pkkoffs+3]+m[j+pkkoffs+4]+m[j+pkkoffs+5]+m[j+pkkoffs+6]+m[j+pkkoffs+7])/8           > pkkpulselevel ? 1 : 0;
        p2 = (uint32_t) (m[j+pkkoffs+8]+m[j+pkkoffs+9]+m[j+pkkoffs+10]+m[j+pkkoffs+11]+m[j+pkkoffs+12]+m[j+pkkoffs+13]+m[j+pkkoffs+14]+m[j+pkkoffs+15])/8   > pkkpulselevel ? 1 : 0;
        p3 = (uint32_t) (m[j+pkkoffs+16]+m[j+pkkoffs+17]+m[j+pkkoffs+18]+m[j+pkkoffs+19]+m[j+pkkoffs+20]+m[j+pkkoffs+21]+m[j+pkkoffs+22]+m[j+pkkoffs+23])/8 > pkkpulselevel ? 1 : 0;
        p4 = (uint32_t) (m[j+pkkoffs+24]+m[j+pkkoffs+25]+m[j+pkkoffs+26]+m[j+pkkoffs+27]+m[j+pkkoffs+28]+m[j+pkkoffs+29]+m[j+pkkoffs+30]+m[j+pkkoffs+31])/8 > pkkpulselevel ? 1 : 0;
        p5 = (uint32_t) (m[j+pkkoffs+32]+m[j+pkkoffs+33]+m[j+pkkoffs+34]+m[j+pkkoffs+35]+m[j+pkkoffs+36]+m[j+pkkoffs+37]+m[j+pkkoffs+38]+m[j+pkkoffs+39])/8 > pkkpulselevel ? 1 : 0;
        p6 = (uint32_t) (m[j+pkkoffs+40]+m[j+pkkoffs+41]+m[j+pkkoffs+42]+m[j+pkkoffs+43]+m[j+pkkoffs+44]+m[j+pkkoffs+45]+m[j+pkkoffs+46]+m[j+pkkoffs+47])/8 > pkkpulselevel ? 1 : 0;

        if( //101
            p1>p2 &&    //10
            p3<p4 &&    //01
            p5>p6       //10
          ) 
        {

    printf("\n__________PROCESSING OK3 CODE__________\n");
    createOK3 (ok3koord, UVD_KOORD_KODE_LEN, 5000);
    oscilloscope(m, ok3koord, j,  UVD_KOORD_KODE_LEN, pulselevel);

    // oscilloscope(ok3koord, 0,  UVD_KOORD_KODE_LEN, pulselevel);             


        okval = decodeKEY(m, j+pkkoffs, pkkpulselevel); // 6 - OK1, 0 - OK2, 5 - OK3

            uint16_t marwrite[UVD_MAX_LEN];

            for(i=0;i<UVD_MAX_LEN;i++) {
            marwrite[i] = m[j+i];
            }

            sprintf(filestr, "ok3-%02d-%02d-%02d-%03d.data", (int) t->tm_hour, (int) t->tm_min, (int) t->tm_sec, (int) tp.tv_usec/1000);
            FILE *f = fopen(filestr, "wb");
            fwrite(marwrite, sizeof(uint16_t), sizeof(marwrite), f);
            fclose(f);


            printf("\n%s - OK3 OK RKK=101 [%d>%d - %d<%d - %d>%d] %d>%d OK3VAL(5)=%d\n",
                timestr,
                m[j+pkkoffs]+m[j+pkkoffs+1]+m[j+pkkoffs+2]+m[j+pkkoffs+3]+m[j+pkkoffs+4]+m[j+pkkoffs+5]+m[j+pkkoffs+6]+m[j+pkkoffs+7],
                m[j+pkkoffs+8]+m[j+pkkoffs+9]+m[j+pkkoffs+10]+m[j+pkkoffs+11]+m[j+pkkoffs+12]+m[j+pkkoffs+13]+m[j+pkkoffs+14]+m[j+pkkoffs+15],
                m[j+pkkoffs+16]+m[j+pkkoffs+17]+m[j+pkkoffs+18]+m[j+pkkoffs+19]+m[j+pkkoffs+20]+m[j+pkkoffs+21]+m[j+pkkoffs+22]+m[j+pkkoffs+23],
                m[j+pkkoffs+24]+m[j+pkkoffs+25]+m[j+pkkoffs+26]+m[j+pkkoffs+27]+m[j+pkkoffs+28]+m[j+pkkoffs+29]+m[j+pkkoffs+30]+m[j+pkkoffs+31],
                m[j+pkkoffs+32]+m[j+pkkoffs+33]+m[j+pkkoffs+34]+m[j+pkkoffs+35]+m[j+pkkoffs+36]+m[j+pkkoffs+37]+m[j+pkkoffs+38]+m[j+pkkoffs+39],
                m[j+pkkoffs+40]+m[j+pkkoffs+41]+m[j+pkkoffs+42]+m[j+pkkoffs+43]+m[j+pkkoffs+44]+m[j+pkkoffs+45]+m[j+pkkoffs+46]+m[j+pkkoffs+47],
                m[j],
                pulselevel,
                okval
                // mediana
                );

            j+=UVD_KOORD_KODE_LEN+UVD_OK3_DELAY+UVD_KEY_KODE_LEN+UVD_INFO_KODE_LEN;
            continue;
        }
            // else printf("OK3 BAD RKK\n");          

    } //end OK3 