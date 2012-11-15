/**
 * The public Diffie-Hellman parameters
 * @file qev.h
 *
 * Generated using: 
 *     openssl dhparam -out dh4096.pem 4096
 *
 * Turned into C code with:
 *     openssl dh -noout -C -in dh4096.pem | indent | expand
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012 Andrew Stone
 *
 * @internal This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#define QEV_DH_NAME_(size) _qev_dh ## size

/**
 * Get the variable name of the DH param
 */
#define QEV_DH_NAME(size) QEV_DH_NAME_(size)

/**
 * The different DH sizes available
 */
#define QEV_DH_SIZES \
	QEV_DH_SIZES_X(1024) \
	QEV_DH_SIZES_X(2048) \
	QEV_DH_SIZES_X(4096)

/**
 * The default DH size to be used when one is not specified
 */
#define QEV_DH_DEFAULT_SIZE 2048

static unsigned char _qev_dh1024_p[] = {
	0xDC, 0xB3, 0xF6, 0xE1, 0x4E, 0xE0, 0xB2, 0x76, 0xE8, 0x86, 0x5D, 0x0A,
	0xD5, 0x03, 0x82, 0xFF, 0x23, 0x22, 0xB2, 0x70, 0xC2, 0xD0, 0x71, 0x42,
	0xCA, 0x03, 0x69, 0x34, 0xF2, 0x89, 0x14, 0xD6, 0x52, 0xC0, 0xB5, 0xA7,
	0xC3, 0x9D, 0xD3, 0xEB, 0x80, 0xF6, 0x10, 0xD7, 0xC7, 0x5E, 0xF4, 0x02,
	0x18, 0xB1, 0xC2, 0x4E, 0xBA, 0x2C, 0x09, 0x0C, 0x8F, 0xC6, 0x96, 0x3C,
	0xC7, 0x5F, 0x04, 0x87, 0x3C, 0x52, 0xEE, 0x9B, 0xDC, 0x6C, 0x73, 0xEF,
	0x8D, 0x5C, 0x15, 0xF0, 0xD5, 0xCC, 0x0C, 0x90, 0x42, 0x18, 0x24, 0x58,
	0x2E, 0xFD, 0xC3, 0xFB, 0xC2, 0x03, 0xCC, 0xC1, 0xBF, 0x25, 0x9E, 0x58,
	0xD3, 0xC8, 0x5D, 0xC7, 0x3C, 0x67, 0xC8, 0xDE, 0x36, 0x5F, 0x60, 0xBC,
	0x95, 0x27, 0x94, 0x1E, 0xCD, 0x58, 0x2E, 0xA6, 0xB2, 0xDE, 0xEA, 0x7C,
	0x37, 0x81, 0xFE, 0x1B, 0x47, 0x72, 0x6F, 0x93,
};

static unsigned char _qev_dh1024_g[] = {
	0x02,
};

static unsigned char _qev_dh2048_p[] = {
	0xE7, 0x7E, 0xAD, 0xC7, 0x89, 0x3B, 0x75, 0x46, 0x1B, 0x21, 0xD3, 0xCE,
	0x1A, 0x3C, 0x1F, 0x1C, 0xD9, 0x28, 0x33, 0x11, 0x0E, 0x2B, 0x32, 0x52,
	0x80, 0x22, 0x8F, 0xAE, 0x30, 0x01, 0x3A, 0x1E, 0xE3, 0x0E, 0x0C, 0x72,
	0x9D, 0x40, 0x52, 0xBF, 0xDA, 0xD4, 0x87, 0x94, 0xDA, 0x52, 0x89, 0x05,
	0x80, 0x9C, 0xA5, 0x3B, 0x17, 0x68, 0x24, 0x1B, 0xC2, 0xDE, 0xDE, 0x41,
	0xED, 0x1D, 0x4B, 0x70, 0xD2, 0x1F, 0x69, 0xB6, 0x61, 0xC3, 0x97, 0x75,
	0x41, 0x09, 0x8E, 0xB0, 0x53, 0xD7, 0xE7, 0xEE, 0x2F, 0xA7, 0xC7, 0x4F,
	0xA1, 0xB2, 0x10, 0xFA, 0x46, 0x12, 0x46, 0x76, 0x3A, 0x22, 0x64, 0x59,
	0x6E, 0xC3, 0xDF, 0x7E, 0xE7, 0x71, 0xBE, 0x36, 0x1A, 0xC1, 0x01, 0xA9,
	0x1D, 0x00, 0xF9, 0x77, 0x74, 0x05, 0x9D, 0x5A, 0xE2, 0x69, 0xAC, 0x1F,
	0x02, 0xC2, 0xB8, 0xF8, 0x05, 0xF6, 0x5A, 0x58, 0x57, 0x93, 0xDC, 0x3C,
	0x11, 0xB3, 0x9D, 0x71, 0x95, 0x0C, 0x99, 0xB2, 0x5C, 0xD1, 0xFD, 0xB2,
	0x61, 0x88, 0x59, 0x44, 0xA3, 0x8C, 0x37, 0x42, 0xAC, 0xE5, 0xE7, 0xBA,
	0x75, 0x8A, 0x31, 0xB7, 0xF7, 0x9B, 0xBC, 0x6B, 0x42, 0xA0, 0x03, 0xA6,
	0x91, 0x0F, 0x1C, 0x09, 0xB1, 0x4C, 0x55, 0x8D, 0x00, 0x04, 0x55, 0x3E,
	0x0F, 0xEC, 0x51, 0xAF, 0xA1, 0xFE, 0xAD, 0x5B, 0x3C, 0x8C, 0xAA, 0x76,
	0x40, 0x00, 0x15, 0xD3, 0x63, 0x23, 0xE5, 0x78, 0x63, 0xD8, 0x48, 0x2B,
	0xF7, 0x39, 0xF1, 0xD2, 0x44, 0xBB, 0x7B, 0xAB, 0x85, 0x74, 0xE0, 0xDC,
	0xDE, 0xE4, 0xDB, 0xC7, 0x20, 0xFE, 0x6C, 0xA7, 0xDD, 0xB3, 0x94, 0x0C,
	0xA4, 0x14, 0xB3, 0x5F, 0xE1, 0xEE, 0x78, 0x02, 0x4E, 0x6C, 0xA5, 0x99,
	0x88, 0xA0, 0xAC, 0x31, 0xB7, 0x6C, 0x19, 0xFC, 0x39, 0xFE, 0x46, 0xE6,
	0xFE, 0xF3, 0xC4, 0x73,
};

static unsigned char _qev_dh2048_g[] = {
	0x02,
};

static unsigned char _qev_dh4096_p[] = {
	0xF4, 0x4C, 0x3E, 0x98, 0x48, 0x05, 0x76, 0x06, 0x57, 0x18, 0x43, 0xE2,
	0x34, 0xCA, 0xDB, 0x7D, 0xD9, 0xA5, 0xDA, 0x54, 0xAE, 0x53, 0x99, 0x4D,
	0xD0, 0xBB, 0x53, 0x00, 0xA4, 0x45, 0x14, 0x3C, 0x03, 0xCA, 0x04, 0xAE,
	0x9E, 0x6A, 0x4C, 0xAE, 0x82, 0x33, 0x4A, 0x05, 0x89, 0x2A, 0x79, 0xFF,
	0xBA, 0x4C, 0xBC, 0x87, 0x84, 0x08, 0xBD, 0x1C, 0xC7, 0xA9, 0x95, 0xA4,
	0x91, 0x88, 0x2B, 0xE3, 0x5A, 0xEB, 0xE2, 0x61, 0xA0, 0x01, 0x0C, 0x4A,
	0x53, 0x04, 0x7A, 0x88, 0x18, 0xB3, 0xB1, 0xD1, 0x43, 0x2E, 0x26, 0x1B,
	0x79, 0x3C, 0x12, 0xAB, 0xF1, 0x93, 0xF0, 0xC2, 0x28, 0xF2, 0xAE, 0x38,
	0xFD, 0xBC, 0x72, 0xAA, 0x39, 0xB3, 0x7F, 0x00, 0x05, 0x00, 0x0D, 0x2E,
	0xA4, 0xDB, 0x48, 0x90, 0x36, 0xEA, 0xAC, 0x65, 0xC1, 0x35, 0x05, 0x64,
	0x20, 0xC2, 0xA2, 0xBA, 0x3A, 0x93, 0x2B, 0xF4, 0xC9, 0x47, 0xE3, 0x0B,
	0xC9, 0x52, 0xAC, 0xFB, 0x83, 0x82, 0xFC, 0x6E, 0xEF, 0xF1, 0x83, 0x97,
	0x91, 0xE8, 0xA9, 0xFD, 0x6B, 0xC3, 0xD2, 0x32, 0x0E, 0xFE, 0x02, 0xE6,
	0xFB, 0x6C, 0xF2, 0x3D, 0xD9, 0x11, 0x9B, 0xF8, 0xFE, 0xE1, 0x1E, 0xE9,
	0xB7, 0xD0, 0xB9, 0x78, 0x84, 0xF5, 0x75, 0x57, 0x55, 0x62, 0x9C, 0x39,
	0x29, 0x4F, 0x33, 0x64, 0xCC, 0xE6, 0xE5, 0xCC, 0x51, 0x66, 0x68, 0x4B,
	0x91, 0x52, 0x23, 0xD6, 0xE8, 0x7D, 0xE5, 0xE9, 0x49, 0x7B, 0x41, 0x79,
	0x3D, 0xAE, 0xC7, 0xE5, 0xF6, 0x6D, 0xAE, 0xF4, 0x69, 0x24, 0xE1, 0xA6,
	0xBB, 0x46, 0x8A, 0x55, 0xF8, 0x5D, 0xC9, 0x17, 0xE5, 0x08, 0x7F, 0xC5,
	0x55, 0x4B, 0xD5, 0xC1, 0xC3, 0xF9, 0xED, 0x29, 0x2C, 0x1D, 0x93, 0x61,
	0x78, 0xD7, 0x13, 0x86, 0x58, 0x81, 0x86, 0x43, 0x01, 0x41, 0x18, 0x45,
	0x81, 0xB0, 0xA6, 0xA3, 0x58, 0xC1, 0x06, 0x96, 0x21, 0x97, 0xC1, 0xB1,
	0x46, 0x31, 0x14, 0xE8, 0x06, 0x02, 0xD1, 0xE5, 0x09, 0xFD, 0xA6, 0xA9,
	0x42, 0x84, 0x77, 0xBD, 0x2B, 0x97, 0x49, 0x1E, 0xFA, 0xE6, 0x74, 0x92,
	0xB6, 0x27, 0x91, 0xBC, 0xB2, 0x2E, 0xE0, 0x51, 0x71, 0x4F, 0x8D, 0x07,
	0x29, 0xF5, 0xF0, 0x0D, 0x02, 0xD3, 0xEC, 0x96, 0x59, 0xE3, 0xBB, 0xB3,
	0x53, 0x8B, 0xB0, 0x48, 0x34, 0x1B, 0xDF, 0xDB, 0xE3, 0x30, 0x2C, 0x72,
	0xD6, 0x95, 0x4D, 0x16, 0xAB, 0xB6, 0x1F, 0xBF, 0x07, 0x5B, 0x52, 0x7C,
	0x6D, 0x52, 0x30, 0x3C, 0x92, 0xE3, 0x2C, 0x58, 0x19, 0x06, 0x12, 0xDA,
	0x15, 0x64, 0xE7, 0x23, 0xB2, 0x27, 0xC1, 0x13, 0xDA, 0xEE, 0xB1, 0x26,
	0x97, 0x47, 0x29, 0x1B, 0xFA, 0xA4, 0x4D, 0xB0, 0x02, 0x43, 0xAB, 0x54,
	0x99, 0xD4, 0x72, 0x75, 0x82, 0x02, 0x50, 0x39, 0xA5, 0x45, 0x2E, 0xC8,
	0x1B, 0xEB, 0x1D, 0xF9, 0xA5, 0x0D, 0xB3, 0xDF, 0x2B, 0x21, 0x04, 0xF3,
	0xBB, 0x2B, 0xE9, 0x31, 0xD9, 0x80, 0xBD, 0xCA, 0xD2, 0xCF, 0x51, 0x5D,
	0x64, 0xE9, 0x66, 0xDA, 0x0D, 0x86, 0xBC, 0x0A, 0x27, 0xEB, 0x03, 0x10,
	0xD0, 0x3F, 0xBF, 0xBF, 0xC7, 0x00, 0xCB, 0x52, 0xA8, 0x9B, 0x82, 0x22,
	0x10, 0xA3, 0xC8, 0xCE, 0x0B, 0xC4, 0xAF, 0x4B, 0xD1, 0x3B, 0xCE, 0x30,
	0xE5, 0x60, 0xEE, 0x7A, 0x58, 0xE0, 0x93, 0x9B, 0x75, 0xE8, 0x40, 0xEB,
	0xB9, 0xCD, 0xAE, 0x05, 0x57, 0x19, 0xBB, 0x23, 0x65, 0x53, 0x37, 0x16,
	0xEC, 0x1C, 0xCA, 0x87, 0xEB, 0x0B, 0xC6, 0x2C, 0xC1, 0xA4, 0x14, 0x6F,
	0x3D, 0x43, 0x95, 0x39, 0xD7, 0x59, 0x3E, 0x69, 0xD0, 0x5B, 0xB6, 0x50,
	0xA8, 0x3C, 0x44, 0xBA, 0x22, 0x16, 0xED, 0x6E, 0x2F, 0x56, 0x6C, 0xF6,
	0xB1, 0xF9, 0x82, 0xF5, 0x53, 0x9A, 0x1C, 0x5B,
};

static unsigned char _qev_dh4096_g[] = {
	0x02,
};