//////////////////////////////////////////////////////////////////////////////////
//
// DCC Smile Function Decoder Do Nothing
// DCC信号を受信するだけで何もしないデコーダ
// [DccCV.H]
// Copyright (c) 2020 Ayanosuke(Maison de DCC) / Desktop Station
// https://desktopstation.net/
//
// This software is released under the MIT License.
// http://opensource.org/licenses/mit-license.php
//
//////////////////////////////////////////////////////////////////////////////////

#ifndef _DccCV_h_
#define _DccCV_h_

#define DEFAULT_DECODER_ADDRESS 3 // HMX 2024-06-02

#define CV_VSTART 2               // CV2
#define CV_ACCRATIO   3           // CV3
#define CV_DECCRATIO  4           // CV4
#define CV_VHIGH 5                // CV5
#define CV60_MASTERVOL 60		      // CV60 HMX
#define CV61_DIR_REV 61           // CV61 HMX
#define CV62_LED_REV 62           // CV62 HMX

#endif
