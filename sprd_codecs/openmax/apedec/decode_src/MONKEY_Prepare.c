
#include "MONKEY_Prepare.h"
#include "MONKEY_APECommon.h"
#include "MONKEY_APEDecompress.h"

static unsigned int CRC32_TABLE[256] = {0UL,1996959894UL,3993919788UL,2567524794UL,124634137UL,1886057615UL,3915621685UL,2657392035UL,249268274UL,2044508324UL,3772115230UL,2547177864UL,162941995UL,2125561021UL,3887607047UL,2428444049UL,498536548UL,1789927666UL,4089016648UL,2227061214UL,450548861UL,1843258603UL,4107580753UL,2211677639UL,325883990UL,1684777152UL,4251122042UL,2321926636UL,335633487UL,1661365465UL,4195302755UL,2366115317UL,997073096UL,1281953886UL,3579855332UL,2724688242UL,1006888145UL,1258607687UL,3524101629UL,2768942443UL,901097722UL,1119000684UL,3686517206UL,2898065728UL,853044451UL,1172266101UL,3705015759UL,2882616665UL,651767980UL,1373503546UL,3369554304UL,3218104598UL,565507253UL,1454621731UL,3485111705UL,3099436303UL,671266974UL,1594198024UL,3322730930UL,2970347812UL,795835527UL,1483230225UL,3244367275UL,3060149565UL,1994146192UL,31158534UL,2563907772UL,4023717930UL,1907459465UL,112637215UL,2680153253UL,3904427059UL,2013776290UL,251722036UL,2517215374UL,3775830040UL,2137656763UL,141376813UL,2439277719UL,3865271297UL,1802195444UL,476864866UL,2238001368UL,
    4066508878UL,1812370925UL,453092731UL,2181625025UL,4111451223UL,1706088902UL,314042704UL,2344532202UL,4240017532UL,1658658271UL,366619977UL,2362670323UL,4224994405UL,1303535960UL,984961486UL,2747007092UL,3569037538UL,1256170817UL,1037604311UL,2765210733UL,3554079995UL,1131014506UL,879679996UL,2909243462UL,3663771856UL,1141124467UL,855842277UL,2852801631UL,3708648649UL,1342533948UL,654459306UL,3188396048UL,3373015174UL,1466479909UL,544179635UL,3110523913UL,3462522015UL,1591671054UL,702138776UL,2966460450UL,3352799412UL,1504918807UL,783551873UL,3082640443UL,3233442989UL,3988292384UL,2596254646UL,62317068UL,1957810842UL,3939845945UL,2647816111UL,81470997UL,1943803523UL,3814918930UL,2489596804UL,225274430UL,2053790376UL,3826175755UL,2466906013UL,167816743UL,2097651377UL,4027552580UL,2265490386UL,503444072UL,1762050814UL,4150417245UL,2154129355UL,426522225UL,1852507879UL,4275313526UL,2312317920UL,282753626UL,1742555852UL,4189708143UL,2394877945UL,397917763UL,1622183637UL,3604390888UL,2714866558UL,953729732UL,1340076626UL,3518719985UL,2797360999UL,1068828381UL,1219638859UL,3624741850UL,
    2936675148UL,906185462UL,1090812512UL,3747672003UL,2825379669UL,829329135UL,1181335161UL,3412177804UL,3160834842UL,628085408UL,1382605366UL,3423369109UL,3138078467UL,570562233UL,1426400815UL,3317316542UL,2998733608UL,733239954UL,1555261956UL,3268935591UL,3050360625UL,752459403UL,1541320221UL,2607071920UL,3965973030UL,1969922972UL,40735498UL,2617837225UL,3943577151UL,1913087877UL,83908371UL,2512341634UL,3803740692UL,2075208622UL,213261112UL,2463272603UL,3855990285UL,2094854071UL,198958881UL,2262029012UL,4057260610UL,1759359992UL,534414190UL,2176718541UL,4139329115UL,1873836001UL,414664567UL,2282248934UL,4279200368UL,1711684554UL,285281116UL,2405801727UL,4167216745UL,1634467795UL,376229701UL,2685067896UL,3608007406UL,1308918612UL,956543938UL,2808555105UL,3495958263UL,1231636301UL,1047427035UL,2932959818UL,3654703836UL,1088359270UL,936918000UL,2847714899UL,3736837829UL,1202900863UL,817233897UL,3183342108UL,3401237130UL,1404277552UL,615818150UL,3134207493UL,3453421203UL,1423857449UL,601450431UL,3009837614UL,3294710456UL,1567103746UL,711928724UL,3020668471UL,3272380065UL,1510334235UL,755167117UL};


void MONKEY_Unprepare(int X, int Y, void * FileInfo, unsigned char * pOutputX, 
			unsigned char * pOutputY, unsigned int * pCRC)
{
    #define CALCULATE_CRC_BYTE_X    *pCRC = (*pCRC >> 8) ^ CRC32_TABLE[(*pCRC & 0xFF) ^ *pOutputX++];
	#define CALCULATE_CRC_BYTE_Y    *pCRC = (*pCRC >> 8) ^ CRC32_TABLE[(*pCRC & 0xFF) ^ *pOutputY++];
    // decompress and convert from (x,y) -> (l,r)
    // sort of long and ugly.... sorry

	MONKEY_APE_DEC_CONFIG *m_APEFileInfo = (MONKEY_APE_DEC_CONFIG *)FileInfo;
	
    if (m_APEFileInfo->nChannels == 2) 
    {
        if (m_APEFileInfo->nBitsPerSample == 16) 
        {
            // get the right and left values
            int nR = X - (Y / 2);
            int nL = nR + Y;

            // error check (for overflows)

            *(int16 *) pOutputX = (int16) nR;
            CALCULATE_CRC_BYTE_X
            CALCULATE_CRC_BYTE_X
                
            *(int16 *) pOutputY = (int16) nL;
            CALCULATE_CRC_BYTE_Y
            CALCULATE_CRC_BYTE_Y
        }
        else if (m_APEFileInfo->nBitsPerSample == 8) 
        {
            unsigned char R = (X - (Y / 2) + 128);
            *pOutputX = R;
            CALCULATE_CRC_BYTE_X
            *pOutputY = (unsigned char) (R + Y);
            CALCULATE_CRC_BYTE_Y
        }
        else if (m_APEFileInfo->nBitsPerSample == 24) 
        {
            int32 RV, LV;
			uint32 nTemp = 0;
            
			RV = X - (Y / 2);
            LV = RV + Y;
            
            if (RV < 0)
                nTemp = ((uint32) (RV + 0x800000)) | 0x800000;
            else
                nTemp = (uint32) RV;    
            
            *pOutputX = (unsigned char) ((nTemp >> 0) & 0xFF);
            CALCULATE_CRC_BYTE_X
            *pOutputX = (unsigned char) ((nTemp >> 8) & 0xFF);
            CALCULATE_CRC_BYTE_X
            *pOutputX = (unsigned char) ((nTemp >> 16) & 0xFF);
            CALCULATE_CRC_BYTE_X

            nTemp = 0;
            if (LV < 0)
                nTemp = ((uint32) (LV + 0x800000)) | 0x800000;
            else
                nTemp = (uint32) LV;    
            
            *pOutputY = (unsigned char) ((nTemp >> 0) & 0xFF);
            CALCULATE_CRC_BYTE_Y
            
            *pOutputY = (unsigned char) ((nTemp >> 8) & 0xFF);
            CALCULATE_CRC_BYTE_Y
            
            *pOutputY = (unsigned char) ((nTemp >> 16) & 0xFF);
            CALCULATE_CRC_BYTE_Y
        }
    }
    else if (m_APEFileInfo->nChannels == 1) 
    {
        if (m_APEFileInfo->nBitsPerSample == 16) 
        {
            int16 R = X;
                
            *(int16 *) pOutputX = (int16) R;
            CALCULATE_CRC_BYTE_X
            CALCULATE_CRC_BYTE_X
        }
        else if (m_APEFileInfo->nBitsPerSample == 8) 
        {
            unsigned char R = X + 128;
            *pOutputX = R;
            CALCULATE_CRC_BYTE_X
        }
        else if (m_APEFileInfo->nBitsPerSample == 24) 
        {
            int32 RV = X;
            
            uint32 nTemp = 0;
            if (RV < 0)
                nTemp = ((uint32) (RV + 0x800000)) | 0x800000;
            else
                nTemp = (uint32) RV;    
            
            *pOutputX = (unsigned char) ((nTemp >> 0) & 0xFF);
            CALCULATE_CRC_BYTE_X
            *pOutputX = (unsigned char) ((nTemp >> 8) & 0xFF);
            CALCULATE_CRC_BYTE_X
            *pOutputX = (unsigned char) ((nTemp >> 16) & 0xFF);
            CALCULATE_CRC_BYTE_X
        }
    }
}
