/* Original test-only LZF grammar validator. No third-party source/dependency.
 * Same exact-size, literal, match-length and backward-distance checks as the
 * independent Python decoder; avoids the GIL/copy cost in a 3x20Hz receiver.
 * Build outputs belong under Saved/Reports, never in the source tree.
 */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

__declspec(dllexport) int pcd_validate_lzf(const unsigned char* data,
    unsigned long long size, unsigned long long expected)
{
    unsigned long long i = 0, written = 0;
    if (expected > 8u * 1024u * 1024u) return 0;
    while (i < size)
    {
        unsigned int control = data[i++], count;
        if (control < 32)
        {
            count = control + 1;
            if (count > size-i || count > expected-written) return 0;
            i += count;
        }
        else
        {
            unsigned int distance;
            count = control >> 5;
            if (count == 7)
            {
                if (i >= size) return 0;
                count += data[i++];
            }
            if (i >= size) return 0;
            distance = ((control & 31) << 8) + data[i++] + 1;
            count += 2;
            if (distance > written || count > expected-written) return 0;
        }
        written += count;
    }
    return written == expected;
}

/* Receiver-only semantic parity with FVirtualPcdDocument::ReadPoints.
 * Thread-local bounded scratch never resides in the measured game process. */
static __declspec(thread) unsigned char* decoded;
static __declspec(thread) unsigned long long capacity;

static int finite_float(const unsigned char* p, float* value)
{
    uint32_t bits; memcpy(&bits,p,4); memcpy(value,p,4);
    return (bits & 0x7f800000u) != 0x7f800000u;
}

__declspec(dllexport) int pcd_validate_records(const unsigned char* data,
    unsigned long long size, unsigned long long points, unsigned int stride,
    unsigned int x, unsigned int y, unsigned int z, unsigned int confidence,
    unsigned int validity, int compressed)
{
    unsigned long long expected, i=0, written=0, p;
    const unsigned char* records=data;
    if(!stride || points > (8u*1024u*1024u)/stride) return 0;
    expected=points*stride;
    if(x>stride || stride-x<4 || y>stride || stride-y<4 || z>stride || stride-z<4) return 0;
    if(confidence!=UINT32_MAX && (confidence>stride || stride-confidence<4)) return 0;
    if(validity!=UINT32_MAX && validity>=stride) return 0;
    if(compressed)
    {
        if(expected>capacity)
        {
            unsigned long long next=65536; void* block;
            while(next<expected) next*=2;
            block=realloc(decoded,(size_t)next); if(!block) return 0;
            decoded=(unsigned char*)block; capacity=next;
        }
        while(i<size)
        {
            unsigned int control=data[i++], count;
            if(control<32)
            {
                count=control+1;
                if(count>size-i || count>expected-written) return 0;
                memcpy(decoded+written,data+i,count); i+=count;
                written+=count;
            }
            else
            {
                unsigned int distance,k;
                count=control>>5;
                if(count==7){if(i>=size)return 0;count+=data[i++];}
                if(i>=size)return 0;
                distance=((control&31)<<8)+data[i++]+1; count+=2;
                if(distance>written || count>expected-written)return 0;
                for(k=0;k<count;++k)decoded[written+k]=decoded[written-distance+k];
                written+=count;
            }
        }
        if(written!=expected)return 0;
        records=decoded;
    }
    else if(size!=expected)return 0;
    for(p=0;p<points;++p)
    {
        float f; unsigned long long base=compressed?p*4:p*stride;
        unsigned long long ox=compressed?(unsigned long long)x*points:x;
        unsigned long long oy=compressed?(unsigned long long)y*points:y;
        unsigned long long oz=compressed?(unsigned long long)z*points:z;
        if(!finite_float(records+base+ox,&f) || !finite_float(records+base+oy,&f) || !finite_float(records+base+oz,&f))return 0;
        if(confidence!=UINT32_MAX)
        {
            unsigned long long offset=compressed?(unsigned long long)confidence*points+p*4:p*stride+confidence;
            if(!finite_float(records+offset,&f) || f<0 || f>1)return 0;
        }
        if(validity!=UINT32_MAX)
        {
            unsigned long long offset=compressed?(unsigned long long)validity*points+p:p*stride+validity;
            if(records[offset]>5)return 0; /* InvalidCalibration is wire enum value5. */
        }
    }
    return 1;
}
