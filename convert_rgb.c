#include <x86intrin.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>

#if !defined(ushort)
#define ushort unsigned short
#endif
#define _(x) x
void derror();
void merror (void *ptr, const char *where);
void pseudoinverse (double (*in)[3], double (*out)[3], int size);
void gamma_curve (double pwr, double ts, int mode, int imax);
int fcol (int row, int col);
extern unsigned *oprof;
extern unsigned filters, colors;
extern int histogram[4][0x2000];
extern const double xyz_rgb[3][3];
extern double gamm[6];
extern unsigned raw_color;
extern float rgb_cam[3][4];
extern int document_mode;
extern int output_color;
extern int verbose;
extern ushort (*image)[4];
extern ushort height, width;

#define FORC(cnt) for (c=0; c < cnt; c++)
#define FORC3 FORC(3)
#define FORCC FORC(colors)
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define LIM(x,min,max) MAX(min,MIN(x,max))
#define CLIP(x) LIM(x,0,65535)

void convert_to_rgb_fast()
{
    unsigned i,j,c;
    int row, col, k;
    ushort *img;
    float out_cam[3][4];
    double num, inverse[3][3];
    static const double xyzd50_srgb[3][3] =
    { { 0.436083, 0.385083, 0.143055 },
      { 0.222507, 0.716888, 0.060608 },
      { 0.013930, 0.097097, 0.714022 } };
    static const double rgb_rgb[3][3] =
    { { 1,0,0 }, { 0,1,0 }, { 0,0,1 } };
    static const double adobe_rgb[3][3] =
    { { 0.715146, 0.284856, 0.000000 },
      { 0.000000, 1.000000, 0.000000 },
      { 0.000000, 0.041166, 0.958839 } };
    static const double wide_rgb[3][3] =
    { { 0.593087, 0.404710, 0.002206 },
      { 0.095413, 0.843149, 0.061439 },
      { 0.011621, 0.069091, 0.919288 } };
    static const double prophoto_rgb[3][3] =
    { { 0.529317, 0.330092, 0.140588 },
      { 0.098368, 0.873465, 0.028169 },
      { 0.016879, 0.117663, 0.865457 } };
    static const double (*out_rgb[])[3] =
    { rgb_rgb, adobe_rgb, wide_rgb, prophoto_rgb, xyz_rgb };
    static const char *name[] =
    { "sRGB", "Adobe RGB (1998)", "WideGamut D65", "ProPhoto D65", "XYZ" };
    static const unsigned phead[] =
    { 1024, 0, 0x2100000, 0x6d6e7472, 0x52474220, 0x58595a20, 0, 0, 0,
      0x61637370, 0, 0, 0x6e6f6e65, 0, 0, 0, 0, 0xf6d6, 0x10000, 0xd32d };
    unsigned pbody[] =
    { 10, 0x63707274, 0, 36, /* cprt */
      0x64657363, 0, 40, /* desc */
      0x77747074, 0, 20, /* wtpt */
      0x626b7074, 0, 20, /* bkpt */
      0x72545243, 0, 14, /* rTRC */
      0x67545243, 0, 14, /* gTRC */
      0x62545243, 0, 14, /* bTRC */
      0x7258595a, 0, 20, /* rXYZ */
      0x6758595a, 0, 20, /* gXYZ */
      0x6258595a, 0, 20 }; /* bXYZ */
    static const unsigned pwhite[] = { 0xf351, 0x10000, 0x116cc };
    unsigned pcurve[] = { 0x63757276, 0, 1, 0x1000000 };

    gamma_curve (gamm[0], gamm[1], 0, 0);
    memcpy (out_cam, rgb_cam, sizeof out_cam);
    raw_color |= colors == 1 || document_mode ||
                 output_color < 1 || output_color > 5;
    if (!raw_color) {
        oprof = (unsigned *) calloc (phead[0], 1);
        merror (oprof, "convert_to_rgb()");
        memcpy (oprof, phead, sizeof phead);
        if (output_color == 5) oprof[4] = oprof[5];
        oprof[0] = 132 + 12*pbody[0];
        for (i=0; i < pbody[0]; i++) {
            oprof[oprof[0]/4] = i ? (i > 1 ? 0x58595a20 : 0x64657363) : 0x74657874;
            pbody[i*3+2] = oprof[0];
            oprof[0] += (pbody[i*3+3] + 3) & -4;
        }
        memcpy (oprof+32, pbody, sizeof pbody);
        oprof[pbody[5]/4+2] = strlen(name[output_color-1]) + 1;
        memcpy ((char *)oprof+pbody[8]+8, pwhite, sizeof pwhite);
        pcurve[3] = (short)(256/gamm[5]+0.5) << 16;
        for (i=4; i < 7; i++)
            memcpy ((char *)oprof+pbody[i*3+2], pcurve, sizeof pcurve);
        pseudoinverse ((double (*)[3])out_rgb[output_color-1], inverse, 3);
        for (i=0; i < 3; i++)
            for (j=0; j < 3; j++) {
                for (num = k=0; k < 3; k++)
                    num += xyzd50_srgb[i][k] * inverse[j][k];
                oprof[pbody[j*3+23]/4+i+2] = num * 0x10000 + 0.5;
            }
        for (i=0; i < phead[0]/4; i++)
            oprof[i] = htonl(oprof[i]);
        strcpy ((char *)oprof+pbody[2]+8, "auto-generated by dcraw");
        strcpy ((char *)oprof+pbody[5]+12, name[output_color-1]);
        for (i=0; i < 3; i++)
            for (j=0; j < colors; j++)
                for (out_cam[i][j] = k=0; k < 3; k++)
                    out_cam[i][j] += out_rgb[output_color-1][i][k] * rgb_cam[k][j];
    }
    if (verbose)
        fprintf (stderr, raw_color ? _("Building histograms...\n") :
                 _("Converting to %s colorspace...\n"), name[output_color-1]);

    memset (histogram, 0, sizeof histogram);
    if(!raw_color) {
        __m128 outcam0= {out_cam[0][0],out_cam[1][0],out_cam[2][0],0},
               outcam1= {out_cam[0][1],out_cam[1][1],out_cam[2][1],0},
               outcam2= {out_cam[0][2],out_cam[1][2],out_cam[2][2],0},
               outcam3= {out_cam[0][3],out_cam[1][3],out_cam[2][3],0};
        for (img=image[0]; img < image[width*height]; img+=4) {
            __m128 out0;
            __m128 vimg0 = {img[0],img[0],img[0],0},
                   vimg1 = {img[1],img[1],img[1],0},
                   vimg2 = {img[2],img[2],img[2],0},
                   vimg3 = {img[3],img[3],img[3],0};

//             out[0] = out_cam[0][0] * img[0]
//                     +out_cam[0][1] * img[1]
//                     +out_cam[0][2] * img[2]
//                     +out_cam[0][3] * img[3];
//             out[1] = out_cam[1][0] * img[0]
//                     +out_cam[1][1] * img[1]
//                     +out_cam[1][2] * img[2]
//                     +out_cam[1][3] * img[3];
//             out[2] = out_cam[2][0] * img[0]
//                     +out_cam[2][1] * img[1]
//                     +out_cam[2][2] * img[2]
//                     +out_cam[2][3] * img[3];
            out0 = _mm_add_ps(_mm_add_ps(
                     _mm_mul_ps(vimg0, outcam0),
                     _mm_mul_ps(vimg1, outcam1)
                   ), _mm_add_ps(
                     _mm_mul_ps(vimg2, outcam2),
                     _mm_mul_ps(vimg3, outcam3)
                   ));
            //clip
            out0 = _mm_max_ps(_mm_set1_ps(0), _mm_min_ps(_mm_set1_ps(0xffff), _mm_round_ps(out0, _MM_FROUND_TO_ZERO)));
            __m128i o = _mm_cvtps_epi32(out0);
            o = _mm_packus_epi32(o,_mm_setzero_si128());
            memcpy(img, &o, sizeof(short)*3);

            FORCC histogram[c][img[c] >> 3]++;
        }

    } else if (document_mode) {
        for (img=image[0], row=0; row < height; row++) {
            for (col=0; col < width; col++, img+=4) {
                img[0] = img[fcol(row,col)];
                FORCC histogram[c][img[c] >> 3]++;
            }
        }
    } else {
        for (img=image[0]; img < image[width*height]; img+=4) {
            FORCC histogram[c][img[c] >> 3]++;
        }
    }
    if (colors == 4 && output_color) colors = 3;
    if (document_mode && filters) colors = 1;
}
