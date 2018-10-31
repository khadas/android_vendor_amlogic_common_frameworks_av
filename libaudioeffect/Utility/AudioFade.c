#define LOG_TAG "audio_fade"

#include <cutils/log.h>
#include "AudioFade.h"


#define TABLE_LENGTH 128

typedef long long int64_t;
typedef unsigned long long uint64_t;
//typedef long int32_t;
//typedef unsigned long uint32_t;
typedef short int16_t;
typedef unsigned short uint16_t;

int mFadeTable_16bit[fadeMax][TABLE_LENGTH] = {
    {/**liner, it will be dynamic generated*/
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    },
    /**fadeInQuad*/
    {
        0x0, 0x4, 0x10, 0x24, 0x41, 0x65, 0x92, 0xc7, 0x104, 0x149, 0x196, 0x1eb, 0x249, 0x2ae, 0x31c, 0x392, 0x410, 0x496, 0x524, 0x5ba,
        0x659, 0x6ff, 0x7ae, 0x865, 0x924, 0x9eb, 0xaba, 0xb92, 0xc71, 0xd59, 0xe48, 0xf40, 0x1040, 0x1148, 0x1259, 0x1371, 0x1491, 0x15ba,
        0x16eb, 0x1824, 0x1965, 0x1aae, 0x1bff, 0x1d58, 0x1eba, 0x2024, 0x2195, 0x230f, 0x2491, 0x261b, 0x27ae, 0x2948, 0x2aeb, 0x2c95, 0x2e48,
        0x3003, 0x31c6, 0x3391, 0x3564, 0x3740, 0x3923, 0x3b0f, 0x3d03, 0x3eff, 0x4103, 0x430f, 0x4523, 0x473f, 0x4964, 0x4b91, 0x4dc5, 0x5002,
        0x5247, 0x5495, 0x56ea, 0x5947, 0x5bad, 0x5e1a, 0x6090, 0x630e, 0x6594, 0x6822, 0x6ab9, 0x6d57, 0x6ffe, 0x72ac, 0x7563, 0x7822, 0x7ae9,
        0x7db8, 0x8090, 0x836f, 0x8657, 0x8946, 0x8c3e, 0x8f3e, 0x9246, 0x9557, 0x986f, 0x9b8f, 0x9eb8, 0xa1e9, 0xa521, 0xa862, 0xabac, 0xaefd,
        0xb256, 0xb5b8, 0xb921, 0xbc93, 0xc00d, 0xc38f, 0xc719, 0xcaab, 0xce45, 0xd1e8, 0xd592, 0xd945, 0xdd00, 0xe0c3, 0xe48e, 0xe861, 0xec3d,
        0xf020, 0xf40c, 0xf800, 0xfbfc, 0x10000,

    },
    /**fadeOutQuad*/
    {
        0x0, 0x403, 0x7ff, 0xbf3, 0xfdf, 0x13c2, 0x179e, 0x1b71, 0x1f3c, 0x22ff, 0x26ba, 0x2a6d, 0x2e17, 0x31ba, 0x3554, 0x38e6, 0x3c70,
        0x3ff2, 0x436c, 0x46de, 0x4a47, 0x4da9, 0x5102, 0x5453, 0x579d, 0x5ade, 0x5e16, 0x6147, 0x6470, 0x6790, 0x6aa8, 0x6db9, 0x70c1, 0x73c1,
        0x76b9, 0x79a8, 0x7c90, 0x7f6f, 0x8247, 0x8516, 0x87dd, 0x8a9c, 0x8d53, 0x9001, 0x92a8, 0x9546, 0x97dd, 0x9a6b, 0x9cf1, 0x9f6f, 0xa1e5,
        0xa452, 0xa6b8, 0xa915, 0xab6a, 0xadb8, 0xaffd, 0xb23a, 0xb46e, 0xb69b, 0xb8c0, 0xbadc, 0xbcf0, 0xbefc, 0xc100, 0xc2fc, 0xc4f0, 0xc6dc,
        0xc8bf, 0xca9b, 0xcc6e, 0xce39, 0xcffc, 0xd1b7, 0xd36a, 0xd514, 0xd6b7, 0xd851, 0xd9e4, 0xdb6e, 0xdcf0, 0xde6a, 0xdfdb, 0xe145, 0xe2a7,
        0xe400, 0xe551, 0xe69a, 0xe7db, 0xe914, 0xea45, 0xeb6e, 0xec8e, 0xeda6, 0xeeb7, 0xefbf, 0xf0bf, 0xf1b7, 0xf2a6, 0xf38e, 0xf46d, 0xf545,
        0xf614, 0xf6db, 0xf79a, 0xf851, 0xf900, 0xf9a6, 0xfa45, 0xfadb, 0xfb69, 0xfbef, 0xfc6d, 0xfce3, 0xfd51, 0xfdb6, 0xfe14, 0xfe69, 0xfeb6,
        0xfefb, 0xff38, 0xff6d, 0xff9a, 0xffbe, 0xffdb, 0xffef, 0xfffb, 0x10000,

    },
    /**fadeInOutQuad*/
    {
        0x0, 0x8, 0x20, 0x49, 0x82, 0xcb, 0x124, 0x18e, 0x208, 0x292, 0x32c, 0x3d7, 0x492, 0x55d, 0x638, 0x724, 0x820, 0x92c, 0xa48, 0xb75,
        0xcb2, 0xdff, 0xf5d, 0x10ca, 0x1248, 0x13d7, 0x1575, 0x1724, 0x18e3, 0x1ab2, 0x1c91, 0x1e81, 0x2081, 0x2291, 0x24b2, 0x26e2, 0x2923,
        0x2b75, 0x2dd6, 0x3048, 0x32ca, 0x355c, 0x37ff, 0x3ab1, 0x3d74, 0x4048, 0x432b, 0x461f, 0x4923, 0x4c37, 0x4f5c, 0x5290, 0x55d6, 0x592b,
        0x5c90, 0x6006, 0x638c, 0x6722, 0x6ac9, 0x6e80, 0x7247, 0x761e, 0x7a06, 0x7dfe, 0x8201, 0x85f9, 0x89e1, 0x8db8, 0x917f, 0x9536, 0x98dd,
        0x9c73, 0x9ff9, 0xa36f, 0xa6d4, 0xaa29, 0xad6f, 0xb0a3, 0xb3c8, 0xb6dc, 0xb9e0, 0xbcd4, 0xbfb7, 0xc28b, 0xc54e, 0xc800, 0xcaa3, 0xcd35,
        0xcfb7, 0xd229, 0xd48a, 0xd6dc, 0xd91d, 0xdb4d, 0xdd6e, 0xdf7e, 0xe17e, 0xe36e, 0xe54d, 0xe71c, 0xe8db, 0xea8a, 0xec28, 0xedb7, 0xef35,
        0xf0a2, 0xf200, 0xf34d, 0xf48a, 0xf5b7, 0xf6d3, 0xf7df, 0xf8db, 0xf9c7, 0xfaa2, 0xfb6d, 0xfc28, 0xfcd3, 0xfd6d, 0xfdf7, 0xfe71, 0xfedb,
        0xff34, 0xff7d, 0xffb6, 0xffdf, 0xfff7, 0x10000,

    },
    /**fadeInCubic*/
    {
        0x0, 0x0, 0x0, 0x0, 0x2, 0x3, 0x6, 0xa, 0x10, 0x17, 0x1f, 0x2a, 0x37, 0x46, 0x57, 0x6b, 0x83, 0x9d, 0xba, 0xdb, 0xff, 0x128, 0x154,
        0x185, 0x1ba, 0x1f3, 0x232, 0x275, 0x2be, 0x30c, 0x35f, 0x3b9, 0x418, 0x47d, 0x4e9, 0x55b, 0x5d4, 0x654, 0x6db, 0x769, 0x7ff, 0x89d,
        0x942, 0x9ef, 0xaa5, 0xb63, 0xc2a, 0xcf9, 0xdd2, 0xeb4, 0xf9f, 0x1094, 0x1192, 0x129b, 0x13ad, 0x14cb, 0x15f2, 0x1725, 0x1862, 0x19aa,
        0x1afe, 0x1c5e, 0x1dc9, 0x1f40, 0x20c3, 0x2252, 0x23ee, 0x2596, 0x274b, 0x290e, 0x2add, 0x2cbb, 0x2ea5, 0x309e, 0x32a4, 0x34b9, 0x36dc,
        0x390e, 0x3b4e, 0x3d9e, 0x3ffc, 0x426a, 0x44e8, 0x4775, 0x4a12, 0x4cc0, 0x4f7d, 0x524c, 0x552b, 0x581a, 0x5b1b, 0x5e2d, 0x6151, 0x6486,
        0x67cd, 0x6b26, 0x6e92, 0x7210, 0x75a0, 0x7943, 0x7cfa, 0x80c3, 0x84a0, 0x8890, 0x8c94, 0x90ad, 0x94d9, 0x991a, 0x9d6f, 0xa1d9, 0xa658,
        0xaaec, 0xaf95, 0xb454, 0xb928, 0xbe12, 0xc313, 0xc82a, 0xcd57, 0xd29b, 0xd7f5, 0xdd67, 0xe2f0, 0xe890, 0xee48, 0xf418, 0xfa00,
        0x10000,
    },
    /**fadeOutCubic*/
    {
        0x0, 0x5ff, 0xbe7, 0x11b7, 0x176f, 0x1d0f, 0x2298, 0x280a, 0x2d64, 0x32a8, 0x37d5, 0x3cec, 0x41ed, 0x46d7, 0x4bab, 0x506a, 0x5513,
        0x59a7, 0x5e26, 0x6290, 0x66e5, 0x6b26, 0x6f52, 0x736b, 0x776f, 0x7b5f, 0x7f3c, 0x8305, 0x86bc, 0x8a5f, 0x8def, 0x916d, 0x94d9, 0x9832,
        0x9b79, 0x9eae, 0xa1d2, 0xa4e4, 0xa7e5, 0xaad4, 0xadb3, 0xb082, 0xb33f, 0xb5ed, 0xb88a, 0xbb17, 0xbd95, 0xc003, 0xc261, 0xc4b1, 0xc6f1,
        0xc923, 0xcb46, 0xcd5b, 0xcf61, 0xd15a, 0xd344, 0xd522, 0xd6f1, 0xd8b4, 0xda69, 0xdc11, 0xddad, 0xdf3c, 0xe0bf, 0xe236, 0xe3a1, 0xe501,
        0xe655, 0xe79d, 0xe8da, 0xea0d, 0xeb34, 0xec52, 0xed64, 0xee6d, 0xef6b, 0xf060, 0xf14b, 0xf22d, 0xf306, 0xf3d5, 0xf49c, 0xf55a, 0xf610,
        0xf6bd, 0xf762, 0xf800, 0xf896, 0xf924, 0xf9ab, 0xfa2b, 0xfaa4, 0xfb16, 0xfb82, 0xfbe7, 0xfc46, 0xfca0, 0xfcf3, 0xfd41, 0xfd8a, 0xfdcd,
        0xfe0c, 0xfe45, 0xfe7a, 0xfeab, 0xfed7, 0xff00, 0xff24, 0xff45, 0xff62, 0xff7c, 0xff94, 0xffa8, 0xffb9, 0xffc8, 0xffd5, 0xffe0, 0xffe8,
        0xffef, 0xfff5, 0xfff9, 0xfffc, 0xfffd, 0xffff, 0xffff, 0xffff, 0x10000,
    },
    /**fadeInOutCubic*/
    {
        0x0, 0x0, 0x1, 0x3, 0x8, 0xf, 0x1b, 0x2b, 0x41, 0x5d, 0x7f, 0xaa, 0xdd, 0x119, 0x15f, 0x1af, 0x20c, 0x274, 0x2ea, 0x36d, 0x3ff, 0x4a1,
        0x552, 0x615, 0x6e9, 0x7cf, 0x8c9, 0x9d6, 0xaf9, 0xc31, 0xd7f, 0xee4, 0x1061, 0x11f7, 0x13a5, 0x156e, 0x1752, 0x1952, 0x1b6e, 0x1da7,
        0x1ffe, 0x2274, 0x2509, 0x27be, 0x2a95, 0x2d8d, 0x30a8, 0x33e6, 0x3749, 0x3ad0, 0x3e7d, 0x4250, 0x464a, 0x4a6c, 0x4eb7, 0x532c, 0x57ca,
        0x5c94, 0x6189, 0x66ab, 0x6bfa, 0x7178, 0x7724, 0x7d00, 0x82ff, 0x88db, 0x8e87, 0x9405, 0x9954, 0x9e76, 0xa36b, 0xa835, 0xacd3, 0xb148,
        0xb593, 0xb9b5, 0xbdaf, 0xc182, 0xc52f, 0xc8b6, 0xcc19, 0xcf57, 0xd272, 0xd56a, 0xd841, 0xdaf6, 0xdd8b, 0xe001, 0xe258, 0xe491, 0xe6ad,
        0xe8ad, 0xea91, 0xec5a, 0xee08, 0xef9e, 0xf11b, 0xf280, 0xf3ce, 0xf506, 0xf629, 0xf736, 0xf830, 0xf916, 0xf9ea, 0xfaad, 0xfb5e, 0xfc00,
        0xfc92, 0xfd15, 0xfd8b, 0xfdf3, 0xfe50, 0xfea0, 0xfee6, 0xff22, 0xff55, 0xff80, 0xffa2, 0xffbe, 0xffd4, 0xffe4, 0xfff0, 0xfff7, 0xfffc,
        0xfffe, 0xffff, 0x10000,

    },
    /**fadeInQuart*/
    {
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x2, 0x3, 0x5, 0x7, 0x9, 0xc, 0x10, 0x15, 0x1a, 0x20, 0x28, 0x30, 0x3b, 0x46, 0x53,
        0x62, 0x73, 0x85, 0x9a, 0xb2, 0xcc, 0xe8, 0x108, 0x12a, 0x150, 0x17a, 0x1a7, 0x1d8, 0x20d, 0x246, 0x284, 0x2c7, 0x30f, 0x35d, 0x3b0,
        0x409, 0x467, 0x4cd, 0x539, 0x5ac, 0x626, 0x6a8, 0x731, 0x7c3, 0x85e, 0x901, 0x9ad, 0xa63, 0xb22, 0xbec, 0xcc0, 0xda0, 0xe8a, 0xf80,
        0x1082, 0x1190, 0x12ac, 0x13d4, 0x150a, 0x164e, 0x17a0, 0x1901, 0x1a72, 0x1bf2, 0x1d82, 0x1f22, 0x20d4, 0x2297, 0x246c, 0x2654, 0x284e,
        0x2a5c, 0x2c7d, 0x2eb3, 0x30fe, 0x335e, 0x35d4, 0x3860, 0x3b03, 0x3dbe, 0x4090, 0x437b, 0x467f, 0x499d, 0x4cd4, 0x5027, 0x5394, 0x571e,
        0x5ac4, 0x5e87, 0x6268, 0x6667, 0x6a84, 0x6ec1, 0x731f, 0x779d, 0x7c3c, 0x80fd, 0x85e1, 0x8ae8, 0x9013, 0x9563, 0x9ad8, 0xa073, 0xa634,
        0xac1d, 0xb22d, 0xb867, 0xbec9, 0xc556, 0xcc0e, 0xd2f1, 0xda01, 0xe13d, 0xe8a7, 0xf040, 0xf808, 0x10000,

    },
    /**fadeOutQuart*/
    {
        0x0, 0x7f7, 0xfbf, 0x1758, 0x1ec2, 0x25fe, 0x2d0e, 0x33f1, 0x3aa9, 0x4136, 0x4798, 0x4dd2, 0x53e2, 0x59cb, 0x5f8c, 0x6527, 0x6a9c,
        0x6fec, 0x7517, 0x7a1e, 0x7f02, 0x83c3, 0x8862, 0x8ce0, 0x913e, 0x957b, 0x9998, 0x9d97, 0xa178, 0xa53b, 0xa8e1, 0xac6b, 0xafd8, 0xb32b,
        0xb662, 0xb980, 0xbc84, 0xbf6f, 0xc241, 0xc4fc, 0xc79f, 0xca2b, 0xcca1, 0xcf01, 0xd14c, 0xd382, 0xd5a3, 0xd7b1, 0xd9ab, 0xdb93, 0xdd68,
        0xdf2b, 0xe0dd, 0xe27d, 0xe40d, 0xe58d, 0xe6fe, 0xe85f, 0xe9b1, 0xeaf5, 0xec2b, 0xed53, 0xee6f, 0xef7d, 0xf07f, 0xf175, 0xf25f, 0xf33f,
        0xf413, 0xf4dd, 0xf59c, 0xf652, 0xf6fe, 0xf7a1, 0xf83c, 0xf8ce, 0xf957, 0xf9d9, 0xfa53, 0xfac6, 0xfb32, 0xfb98, 0xfbf6, 0xfc4f, 0xfca2,
        0xfcf0, 0xfd38, 0xfd7b, 0xfdb9, 0xfdf2, 0xfe27, 0xfe58, 0xfe85, 0xfeaf, 0xfed5, 0xfef7, 0xff17, 0xff33, 0xff4d, 0xff65, 0xff7a, 0xff8c,
        0xff9d, 0xffac, 0xffb9, 0xffc4, 0xffcf, 0xffd7, 0xffdf, 0xffe5, 0xffea, 0xffef, 0xfff3, 0xfff6, 0xfff8, 0xfffa, 0xfffc, 0xfffd, 0xfffe,
        0xfffe, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x10000,
    },
    /**fadeInOutQuart*/
    {
        0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x2, 0x4, 0x8, 0xd, 0x14, 0x1d, 0x29, 0x39, 0x4d, 0x66, 0x84, 0xa8, 0xd3, 0x106, 0x142, 0x187, 0x1d8,
        0x233, 0x29c, 0x313, 0x398, 0x42f, 0x4d6, 0x591, 0x660, 0x745, 0x841, 0x956, 0xa85, 0xbd0, 0xd39, 0xec1, 0x106a, 0x1236, 0x1427,
        0x163e, 0x187f, 0x1aea, 0x1d81, 0x2048, 0x233f, 0x266a, 0x29ca, 0x2d62, 0x3134, 0x3542, 0x398f, 0x3e1e, 0x42f0, 0x4809, 0x4d6c, 0x531a,
        0x5916, 0x5f64, 0x6607, 0x6d00, 0x7453, 0x7c04, 0x83fb, 0x8bac, 0x92ff, 0x99f8, 0xa09b, 0xa6e9, 0xace5, 0xb293, 0xb7f6, 0xbd0f, 0xc1e1,
        0xc670, 0xcabd, 0xcecb, 0xd29d, 0xd635, 0xd995, 0xdcc0, 0xdfb7, 0xe27e, 0xe515, 0xe780, 0xe9c1, 0xebd8, 0xedc9, 0xef95, 0xf13e, 0xf2c6,
        0xf42f, 0xf57a, 0xf6a9, 0xf7be, 0xf8ba, 0xf99f, 0xfa6e, 0xfb29, 0xfbd0, 0xfc67, 0xfcec, 0xfd63, 0xfdcc, 0xfe27, 0xfe78, 0xfebd, 0xfef9,
        0xff2c, 0xff57, 0xff7b, 0xff99, 0xffb2, 0xffc6, 0xffd6, 0xffe2, 0xffeb, 0xfff2, 0xfff7, 0xfffb, 0xfffd, 0xfffe, 0xffff, 0xffff, 0xffff,
        0xffff, 0x10000,

    },
    /**fadeInQuint*/
    {
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x2, 0x2, 0x3, 0x4, 0x6, 0x8, 0xa, 0xc, 0xf, 0x13,
        0x17, 0x1c, 0x22, 0x28, 0x30, 0x38, 0x42, 0x4d, 0x5a, 0x68, 0x77, 0x89, 0x9d, 0xb2, 0xcb, 0xe5, 0x103, 0x123, 0x147, 0x16e, 0x198,
        0x1c6, 0x1f9, 0x230, 0x26b, 0x2ac, 0x2f2, 0x33d, 0x38e, 0x3e6, 0x444, 0x4a9, 0x515, 0x58a, 0x606, 0x68b, 0x719, 0x7b0, 0x851, 0x8fd,
        0x9b4, 0xa76, 0xb44, 0xc1e, 0xd05, 0xdfa, 0xefe, 0x1010, 0x1131, 0x1263, 0x13a5, 0x14f9, 0x165f, 0x17d7, 0x1963, 0x1b04, 0x1cba,
        0x1e85, 0x2067, 0x2261, 0x2473, 0x269e, 0x28e4, 0x2b44, 0x2dc1, 0x305a, 0x3311, 0x35e7, 0x38dd, 0x3bf4, 0x3f2e, 0x428a, 0x460a, 0x49b0,
        0x4d7c, 0x5170, 0x558c, 0x59d3, 0x5e45, 0x62e4, 0x67b1, 0x6cad, 0x71da, 0x7738, 0x7cca, 0x8291, 0x888e, 0x8ec3, 0x9531, 0x9bd9, 0xa2bf,
        0xa9e2, 0xb144, 0xb8e8, 0xc0cf, 0xc8fa, 0xd16b, 0xda25, 0xe328, 0xec77, 0xf614, 0x10000,

    },
    /**fadeOutQuint*/
    {
        0x0, 0x9eb, 0x1388, 0x1cd7, 0x25da, 0x2e94, 0x3705, 0x3f30, 0x4717, 0x4ebb, 0x561d, 0x5d40, 0x6426, 0x6ace, 0x713c, 0x7771, 0x7d6e,
        0x8335, 0x88c7, 0x8e25, 0x9352, 0x984e, 0x9d1b, 0xa1ba, 0xa62c, 0xaa73, 0xae8f, 0xb283, 0xb64f, 0xb9f5, 0xbd75, 0xc0d1, 0xc40b, 0xc722,
        0xca18, 0xccee, 0xcfa5, 0xd23e, 0xd4bb, 0xd71b, 0xd961, 0xdb8c, 0xdd9e, 0xdf98, 0xe17a, 0xe345, 0xe4fb, 0xe69c, 0xe828, 0xe9a0, 0xeb06,
        0xec5a, 0xed9c, 0xeece, 0xefef, 0xf101, 0xf205, 0xf2fa, 0xf3e1, 0xf4bb, 0xf589, 0xf64b, 0xf702, 0xf7ae, 0xf84f, 0xf8e6, 0xf974, 0xf9f9,
        0xfa75, 0xfaea, 0xfb56, 0xfbbb, 0xfc19, 0xfc71, 0xfcc2, 0xfd0d, 0xfd53, 0xfd94, 0xfdcf, 0xfe06, 0xfe39, 0xfe67, 0xfe91, 0xfeb8, 0xfedc,
        0xfefc, 0xff1a, 0xff34, 0xff4d, 0xff62, 0xff76, 0xff88, 0xff97, 0xffa5, 0xffb2, 0xffbd, 0xffc7, 0xffcf, 0xffd7, 0xffdd, 0xffe3, 0xffe8,
        0xffec, 0xfff0, 0xfff3, 0xfff5, 0xfff7, 0xfff9, 0xfffb, 0xfffc, 0xfffd, 0xfffd, 0xfffe, 0xfffe, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
        0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x10000,

    },
    /**fadeInOutQuint*/
    {
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x3, 0x5, 0x7, 0xb, 0x11, 0x18, 0x21, 0x2d, 0x3b, 0x4e, 0x65, 0x81, 0xa3, 0xcc, 0xfc,
        0x135, 0x179, 0x1c7, 0x222, 0x28a, 0x303, 0x38c, 0x428, 0x4da, 0x5a2, 0x682, 0x77f, 0x898, 0x9d2, 0xb2f, 0xcb1, 0xe5d, 0x1033, 0x1239,
        0x1472, 0x16e0, 0x1988, 0x1c6e, 0x1f97, 0x2305, 0x26be, 0x2ac6, 0x2f22, 0x33d8, 0x38ed, 0x3e65, 0x4447, 0x4a98, 0x515f, 0x58a2, 0x6067,
        0x68b5, 0x7194, 0x7b0a, 0x84f5, 0x8e6b, 0x974a, 0x9f98, 0xa75d, 0xaea0, 0xb567, 0xbbb8, 0xc19a, 0xc712, 0xcc27, 0xd0dd, 0xd539, 0xd941,
        0xdcfa, 0xe068, 0xe391, 0xe677, 0xe91f, 0xeb8d, 0xedc6, 0xefcc, 0xf1a2, 0xf34e, 0xf4d0, 0xf62d, 0xf767, 0xf880, 0xf97d, 0xfa5d, 0xfb25,
        0xfbd7, 0xfc73, 0xfcfc, 0xfd75, 0xfddd, 0xfe38, 0xfe86, 0xfeca, 0xff03, 0xff33, 0xff5c, 0xff7e, 0xff9a, 0xffb1, 0xffc4, 0xffd2, 0xffde,
        0xffe7, 0xffee, 0xfff4, 0xfff8, 0xfffa, 0xfffc, 0xfffe, 0xfffe, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x10000,

    }
};

int fadeNext(fadeMethod fade_method, int t, int b, int c, int d)
{
    int e;

    switch (fade_method) {
    default:
    case fadeLinear:
        return ((int64_t)c * t) / d + b;
    case fadeInQuad:
    case fadeOutQuad:
    case fadeInOutQuad:
    case fadeInCubic:
    case fadeOutCubic:
    case fadeInOutCubic:
    case fadeInQuart:
    case fadeOutQuart:
    case fadeInOutQuart:
    case fadeInQuint:
    case fadeOutQuint:
    case fadeInOutQuint:
        e = fade_method;
        uint32_t inter = ((uint64_t)t << 16) / (d - 1);
        uint32_t index = inter * (TABLE_LENGTH - 1);
        uint16_t x1 = index >> 16;  // 16
        uint16_t x2 = index & 0xffff;
        uint32_t y1 = mFadeTable_16bit[e][x1];
        uint32_t y3 = mFadeTable_16bit[e][x1 + 1];
        if ((x1 + 1) >= TABLE_LENGTH) {
            y3 = y1;
        }

        uint32_t y2 = (x2 * (y3 - y1) >> 16) + y1;

        int ret = (((uint64_t)c * y2) >> 16) + b;
        return ret;
    }
}


int AudioFadeBuf(AudioFade_t *pAudFade, void *rawBuf, unsigned int nSamples)
{
    //int** outbuf;
    int16_t * pOut16bit;
    int32_t tmp32;
    unsigned int nchannels;
    unsigned int i, j;
    int delta = 0;

    // currently fix to 16bits
    pOut16bit = (int16_t *)rawBuf;
    nchannels = pAudFade->channels;

    pAudFade->mfadeFramesTotal = ((long long)pAudFade->mfadeTimeTotal * pAudFade->samplingRate) / 1000;

    delta = pAudFade->mTargetVolume - pAudFade->mStartVolume;
    ALOGI("%s,mfadeFramesTotal=%d delta=%d,samplingRate = %d,channels = %d,format = %d\n", __FUNCTION__,
          pAudFade->mfadeFramesTotal,
          delta,
          pAudFade->samplingRate,
          pAudFade->channels,
          pAudFade->format);

    for (i = 0; i < nSamples; i++) {
        if (pAudFade->mfadeFramesTotal == 0) {
            pAudFade->mCurrentVolume = pAudFade->mTargetVolume;
        } else if (pAudFade->mfadeFramesUsed < pAudFade->mfadeFramesTotal) {
            // caculate next volume
            pAudFade->mCurrentVolume = fadeNext(pAudFade->mfadeMethod, pAudFade->mfadeFramesUsed,
                                                pAudFade->mStartVolume, delta, pAudFade->mfadeFramesTotal - 1);
            pAudFade->mfadeFramesUsed++;
        }
        for (j = 0; j < nchannels; j++) {
            tmp32 = pOut16bit[i * nchannels + j] * pAudFade->mCurrentVolume;
            pOut16bit[i * nchannels + j] = tmp32 >> 16;
        }
    }
    // ALOGI("mCurrentVolume=%d\n",pAudFade->mCurrentVolume);
    return AUD_FADE_OK;
}

void mutePCMBuf(AudioFade_t *pAudFade, void *rawBuf, unsigned int nSamples)
{
    int16_t * pOut16bit;
    unsigned int i, j;
    unsigned int nchannels;

    // currently fix to 16bits
    pOut16bit = (int16_t *)rawBuf;
    nchannels = pAudFade->channels;

    for (i = 0; i < nSamples; i++) {
        for (j = 0; j < nchannels; j++) {
            pOut16bit[i * nchannels + j] = 0;
        }
    }
}

// fadeMs: time interval need to do fade in/fade out (in million seconds)
// muteFrms: after fade out, how many audio frames need to mute (default value 0)
int AudioFadeInit(AudioFade_t *pAudFade, fadeMethod fade_method, int fadeMs, int muteFrms)
{
    if (!pAudFade) {
        ALOGI("%s,pAudFade is NULL", __FUNCTION__);
        return AUD_FADE_ERR;
    }

    pAudFade->mfadeMethod = fade_method;
    pAudFade->mfadeTimeTotal = fadeMs;
    pAudFade->muteCounts = muteFrms;
    ALOGI("%s,fade method = %d, fadeMs = %d, muteFrms = %d", __FUNCTION__, fade_method, fadeMs, muteFrms);

    return AUD_FADE_OK;
}

int AudioFadeSetDirection(AudioFade_t *pAudFade, fade_direction_e eFadeDirection)
{
    if (!pAudFade) {
        ALOGE("%s,pAudFade is NULL", __FUNCTION__);
        return AUD_FADE_ERR;
    }

    pAudFade->mfadeDirction = eFadeDirection;

    if (pAudFade->mfadeDirction == FADE_AUD_IN) {
        pAudFade->mTargetVolume = 1 << 16;
        pAudFade->mStartVolume = 0;
        pAudFade->mCurrentVolume = 0;
    } else if (pAudFade->mfadeDirction == FADE_AUD_OUT) {
        pAudFade->mTargetVolume = 0;
        pAudFade->mStartVolume = 1 << 16;
        pAudFade->mCurrentVolume = 1 << 16;
    } else {
        ALOGE("%s,fade dirction is %d", __FUNCTION__, eFadeDirection);
        return AUD_FADE_ERR;
    }

    ALOGI("%s,fade dirction is %d", __FUNCTION__, eFadeDirection);
    return AUD_FADE_OK;
}


int AudioFadeSetState(AudioFade_t *pAudFade, fade_state_e fadeState)
{
    if (!pAudFade) {
        ALOGI("%s,pAudFade is NULL", __FUNCTION__);
        return AUD_FADE_ERR;
    }

    pAudFade->mFadeState = fadeState;
    ALOGI("%s,fade mFadeState = %d", __FUNCTION__, fadeState);

    return AUD_FADE_OK;
}

int AudioFadeSetFormat(AudioFade_t *pAudFade,
                       uint32_t        samplingRate,    // sampling rate
                       uint32_t        channels,        // number of channels
                       unsigned int    format      // Audio format (see audio_format_t in audio.h)
                      )
{
    if (!pAudFade) {
        ALOGI("%s,pAudFade is NULL", __FUNCTION__);
        return AUD_FADE_ERR;
    }

    pAudFade->samplingRate = samplingRate;
    pAudFade->channels = channels;
    pAudFade->format = format;
    ALOGI("%s,samplingRate = %d,channels = %d,format = %d", __FUNCTION__, samplingRate, channels, format);

    return AUD_FADE_OK;
}


int AudioFadeProcessType1(AudioFade_t *pAudFade, void *pInput, unsigned int nSamples)
{
    if (!pAudFade) {
        ALOGE("%s,pAudFade is NULL", __FUNCTION__);
        return AUD_FADE_ERR;
    }

    // do audio traisition
    switch (pAudFade->mFadeState) {
    case AUD_FADE_OUT_START: {
        pAudFade->mTargetVolume = 0;
        pAudFade->mStartVolume = 1 << 16;
        pAudFade->mCurrentVolume = 1 << 16;
        pAudFade->mfadeTimeUsed = 0;
        pAudFade->mfadeTimeTotal = DEFAULT_FADE_OUT_MS;
        pAudFade->mfadeFramesUsed = 0;
        pAudFade->muteCounts = 1;
        AudioFadeBuf(pAudFade, pInput, nSamples);
        pAudFade->mFadeState = AUD_FADE_OUT;
    }
    break;
    case AUD_FADE_OUT: {
        // do fade out process
        if (pAudFade->mCurrentVolume != 0) {
            AudioFadeBuf(pAudFade, pInput, nSamples);
        } else {
            pAudFade->mFadeState = AUD_FADE_MUTE;
        }

    }
    break;
    case AUD_FADE_MUTE: {
        if (pAudFade->muteCounts <= 0) {
            pAudFade->mFadeState = AUD_FADE_IN;
            // slowly increase audio volume
            pAudFade->mTargetVolume = 1 << 16;
            pAudFade->mStartVolume = 0;
            pAudFade->mCurrentVolume = 0;
            pAudFade->mfadeTimeUsed = 0;
            pAudFade->mfadeFramesUsed = 0;
            pAudFade->mfadeTimeTotal = DEFAULT_FADE_IN_MS;
        } else {
            mutePCMBuf(pAudFade, pInput, nSamples);
            pAudFade->muteCounts--;
        }
    }
    break;

    case AUD_FADE_IN: {
        AudioFadeBuf(pAudFade, pInput, nSamples);
        if (pAudFade->mCurrentVolume == 1 << 16) {
            pAudFade->mFadeState = AUD_FADE_IDLE;
        }
    }
    break;
    case AUD_FADE_IDLE:
        // do nothing
        break;
    default:
        break;
    }

    return AUD_FADE_OK;
}
