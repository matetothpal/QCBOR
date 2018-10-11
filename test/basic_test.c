/*==============================================================================
 basic_test.c -- most basic test for QCBOR
 
 Copyright 2018 Laurence Lundblade
 
 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:
 
 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 
 (This is the MIT license)
 ==============================================================================*/
//  Created by Laurence Lundblade on 9/13/18.


#include "basic_test.h"
#include "qcbor.h"


/*
 Some very minimal tests until the full test suite is open sourced and available.
 Return codes here don't mean much (yet).
 */
int basic_test_one()
{
    // Very simple CBOR, a map with one boolean that is true in it
    UsefulBuf_MakeStackUB(MemoryForEncoded, 100);
    QCBOREncodeContext EC;
    
    QCBOREncode_Init(&EC, MemoryForEncoded);
    
    QCBOREncode_OpenMap(&EC);
    QCBOREncode_AddBoolToMapN(&EC, 66, true);
    QCBOREncode_CloseMap(&EC);
    
    UsefulBufC Encoded;
    if(QCBOREncode_Finish2(&EC, &Encoded)) {
        return -3;
    }
    
    
    // Decode it and see that is right
    QCBORDecodeContext DC;
    QCBORItem Item;
    QCBORDecode_Init(&DC, Encoded, QCBOR_DECODE_MODE_NORMAL);
    
    QCBORDecode_GetNext(&DC, &Item);
    if(Item.uDataType != QCBOR_TYPE_MAP) {
        return -1;
    }

    QCBORDecode_GetNext(&DC, &Item);
    if(Item.uDataType != QCBOR_TYPE_TRUE) {
        return -1;
    }
    
    if(QCBORDecode_Finish(&DC)) {
        return -2;
    }
    
    
    // Make another encoded message with the CBOR from the previous put into this one
    UsefulBuf_MakeStackUB(MemoryForEncoded2, 100);
    QCBOREncode_Init(&EC, MemoryForEncoded2);
    QCBOREncode_OpenArray(&EC);
    QCBOREncode_AddUInt64(&EC, 451);
    QCBOREncode_AddEncoded(&EC, Encoded);
    QCBOREncode_OpenMap(&EC);
    QCBOREncode_AddEncodedToMapN(&EC, -70000, Encoded);
    QCBOREncode_CloseMap(&EC);
    QCBOREncode_CloseArray(&EC);
    
    UsefulBufC Encoded2;
    if(QCBOREncode_Finish2(&EC, &Encoded2)) {
        return -3;
    }
    /*
     [                // 0    1:3
        451,          // 1    1:2
        {             // 1    1:2   2:1
          66: true    // 2    1:1
        },
        {             // 1    1:1   2:1
          -70000: {   // 2    1:1   2:1   3:1
            66: true  // 3    XXXXXX
          }
        }
     ]
     
     
     
      83                # array(3)
         19 01C3        # unsigned(451)
         A1             # map(1)
            18 42       # unsigned(66)
            F5          # primitive(21)
         A1             # map(1)
            3A 0001116F # negative(69999)
            A1          # map(1)
               18 42    # unsigned(66)
               F5       # primitive(21)
     */
    
    // Decode it and see if it is OK
    QCBORDecode_Init(&DC, Encoded2, QCBOR_DECODE_MODE_NORMAL);

    // 0    1:3
    QCBORDecode_GetNext(&DC, &Item);
    if(Item.uDataType != QCBOR_TYPE_ARRAY || Item.val.uCount != 3) {
        return -1;
    }
   
    // 1    1:2
    QCBORDecode_GetNext(&DC, &Item);
    if(Item.uDataType != QCBOR_TYPE_INT64 || Item.val.uint64 != 451) {
        return -1;
    }

    // 1    1:2   2:1
    QCBORDecode_GetNext(&DC, &Item);
    if(Item.uDataType != QCBOR_TYPE_MAP || Item.val.uCount != 1) {
        return -1;
    }
   
    // 2    1:1
    QCBORDecode_GetNext(&DC, &Item);
    if(Item.uDataType != QCBOR_TYPE_TRUE) {
        return -1;
    }

    // 1    1:1   2:1
    QCBORDecode_GetNext(&DC, &Item);
    if(Item.uDataType != QCBOR_TYPE_MAP || Item.val.uCount != 1) {
        return -1;
    }
   
    // 2    1:1   2:1   3:1
    QCBORDecode_GetNext(&DC, &Item);
    if(Item.uDataType != QCBOR_TYPE_MAP || Item.val.uCount != 1 || Item.uLabelType != QCBOR_TYPE_INT64 || Item.label.int64 != -70000) {
        return -1;
    }

    // 3    XXXXXX
    QCBORDecode_GetNext(&DC, &Item);
    if(Item.uDataType != QCBOR_TYPE_TRUE || Item.uLabelType != QCBOR_TYPE_INT64 || Item.label.int64 != 66) {
        return -1;
    }
    
    if(QCBORDecode_Finish(&DC)) {
        return -2;
    }
   
    return 0;
}



static const uint8_t pIndefiniteLenString[] = {
   0x81, // Array of length one
   0x7f, // text string marked with indefinite length
      0x65, 0x73, 0x74, 0x72, 0x65, 0x61, // first segment
      0x64, 0x6d, 0x69, 0x6e, 0x67, // second segment
   0xff // ending break
};

static const uint8_t pIndefiniteArray[] = {0x9f, 0x01, 0x82, 0x02, 0x03, 0xff};

// [1, [2, 3]]


//0x9f018202039f0405ffff

int indefinite_length_decode_test()
{
    UsefulBufC IndefLen = UsefulBuf_FromByteArrayLiteral(pIndefiniteArray);
    
    
    // Decode it and see if it is OK
   UsefulBuf_MakeStackUB(MemPool, 200);
    QCBORDecodeContext DC;
    QCBORItem Item;
    QCBORDecode_Init(&DC, IndefLen, QCBOR_DECODE_MODE_NORMAL);
   
   QCBORDecode_SetMemPool(&DC, MemPool, false);

   
    QCBORDecode_GetNext(&DC, &Item);
    if(Item.uDataType != QCBOR_TYPE_ARRAY) {
        return -1;
    }

   QCBORDecode_GetNext(&DC, &Item);
   if(Item.uDataType != QCBOR_TYPE_INT64) {
      return -1;
   }
   
    QCBORDecode_GetNext(&DC, &Item);
    if(Item.uDataType != QCBOR_TYPE_ARRAY) {
        return -1;
    }
    
    QCBORDecode_GetNext(&DC, &Item);
    if(Item.uDataType != QCBOR_TYPE_INT64) {
        return -1;
    }
    
    QCBORDecode_GetNext(&DC, &Item);
    if(Item.uDataType != QCBOR_TYPE_INT64) {
        return -1;
    }
   
   if(QCBORDecode_Finish(&DC)) {
      return -2;
   }
    
    return 0;
}

int indefinite_length_decode_string_test() {
   UsefulBufC IndefLen = UsefulBuf_FromByteArrayLiteral(pIndefiniteLenString);
   
   
   // Decode it and see if it is OK
   QCBORDecodeContext DC;
   QCBORItem Item;
   UsefulBuf_MakeStackUB(MemPool, 200);

   QCBORDecode_Init(&DC, IndefLen, QCBOR_DECODE_MODE_NORMAL);
   
   QCBORDecode_SetMemPool(&DC,  MemPool, false);

   
   QCBORDecode_GetNext(&DC, &Item);
   if(Item.uDataType != QCBOR_TYPE_ARRAY) {
      return -1;
   }
   
   QCBORDecode_GetNext(&DC, &Item);
   if(Item.uDataType != QCBOR_TYPE_TEXT_STRING) {
      return -1;
   }
   
   return 0;
}

