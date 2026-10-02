#pragma once
#include "CoreMinimal.h"

namespace CameraFrameMarker
{
constexpr int32 Columns=10,Rows=8,Count=Columns*Rows;
inline uint16 Crc(uint64 Token)
{
    uint16 V=0xffff;
    for(int32 Byte=0;Byte<8;++Byte){V^=uint16((Token>>(Byte*8))&255)<<8;for(int32 Bit=0;Bit<8;++Bit)V=(V&0x8000)?uint16((V<<1)^0x1021):uint16(V<<1);}
    return V;
}
inline bool Cell(uint64 Token,int32 Index){return Index<64?((Token>>Index)&1)!=0:((Crc(Token)>>(Index-64))&1)!=0;}
inline FVector CellLocation(int32 Index){return FVector(400,(Index%Columns-(Columns-1)*.5)*24,100+((Rows-1)*.5-Index/Columns)*24);}
inline bool Decode(const TArray64<uint8>& Bgra,int32 Width,int32 Height,const FTransform& Pose,float Fov,uint64& Out)
{
    if(Bgra.Num()!=int64(Width)*Height*4)return false;
    uint64 Token=0;uint16 Code=0;
    const double Focal=Width/(2*FMath::Tan(FMath::DegreesToRadians(double(Fov)*.5)));
    for(int32 I=0;I<Count;++I)
    {
        FVector P=Pose.InverseTransformPosition(CellLocation(I));P.X-=2.5;
        if(P.X<=0)return false;
        const int32 X=FMath::RoundToInt(Width*.5+P.Y/P.X*Focal),Y=FMath::RoundToInt(Height*.5-P.Z/P.X*Focal);
        if(X<2||Y<2||X>=Width-2||Y>=Height-2)return false;
        int32 Sum=0;for(int32 Dy=-1;Dy<=1;++Dy)for(int32 Dx=-1;Dx<=1;++Dx)Sum+=Bgra[(int64(Y+Dy)*Width+X+Dx)*4+2];
        if(Sum>9*128){if(I<64)Token|=uint64(1)<<I;else Code|=uint16(1)<<(I-64);}
    }
    Out=Token;return Crc(Token)==Code;
}
}
