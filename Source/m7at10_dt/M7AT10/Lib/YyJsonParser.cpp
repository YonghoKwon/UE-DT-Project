#include "YyJsonParser.h"

FYyJsonParser::FYyJsonParser()
{
    Doc = nullptr;
    Root = nullptr;
}

FYyJsonParser::~FYyJsonParser()
{
    // 소멸자에서 메모리 해제
    if (Doc)
    {
        yyjson_doc_free(Doc);
        Doc = nullptr;
    }
}

bool FYyJsonParser::JsonParse(const FString& JsonContent)
{
    // 기존 문서가 있다면 해제
    if (Doc)
    {
        yyjson_doc_free(Doc);
        Doc = nullptr;
        Root = nullptr;
    }

    if (JsonContent.IsEmpty())
    {
        return false;
    }

    // FString(TCHAR) -> UTF-8 변환
    // yyjson은 UTF-8을 기본으로 사용합니다.
    FTCHARToUTF8 Utf8String(*JsonContent);

    // JSON 파싱 (Read)
    Doc = yyjson_read(Utf8String.Get(), Utf8String.Length(), 0);

    if (Doc)
    {
        Root = yyjson_doc_get_root(Doc);
        return true;
    }

    return false;
}

yyjson_val* FYyJsonParser::JsonParsePtr(const FString& Path) const
{
    if (!Doc) return nullptr;

    FTCHARToUTF8 Utf8Path(*Path);
    return yyjson_doc_ptr_get(Doc, Utf8Path.Get());
}

yyjson_val* FYyJsonParser::JsonParseKeyword(yyjson_val* ObjectVal, const FString& Key) const
{
    // ObjectVal이 nullptr이면 Root에서 검색
    yyjson_val* TargetObj = ObjectVal ? ObjectVal : Root;

    if (!TargetObj || !yyjson_is_obj(TargetObj))
    {
        return nullptr;
    }

    FTCHARToUTF8 Utf8Key(*Key);
    return yyjson_obj_get(TargetObj, Utf8Key.Get());
}

yyjson_val* FYyJsonParser::JsonParseArr(yyjson_val* ArrayVal) const
{
    if (ArrayVal && yyjson_is_arr(ArrayVal))
    {
        return ArrayVal;
    }
    return nullptr;
}

yyjson_val* FYyJsonParser::JsonParseArrIndex(yyjson_val* ArrayVal, int32 Index) const
{
    if (ArrayVal && yyjson_is_arr(ArrayVal))
    {
        return yyjson_arr_get(ArrayVal, Index);
    }
    return nullptr;
}

FString FYyJsonParser::GetString(yyjson_val* Val, const FString& DefaultValue) const
{
    if (Val && yyjson_is_str(Val))
    {
        // UTF-8 -> FString(TCHAR) 변환
        return FString(UTF8_TO_TCHAR(yyjson_get_str(Val)));
    }
    return DefaultValue;
}

int32 FYyJsonParser::GetInt(yyjson_val* Val, int32 DefaultValue) const
{
    if (!Val) return DefaultValue;

    if (yyjson_is_int(Val))
    {
        return yyjson_get_int(Val);
    }

    if (yyjson_is_str(Val))
    {
        const char* RawStr = yyjson_get_str(Val);
        if (RawStr)
        {
            return FCString::Atoi(UTF8_TO_TCHAR(RawStr));
        }
    }

    return DefaultValue;
}

int64 FYyJsonParser::GetInt64(yyjson_val* Val, int64 DefaultValue) const
{
    if (!Val) return DefaultValue;

    if (yyjson_is_int(Val))
    {
        return yyjson_get_sint(Val);
    }

    if (yyjson_is_str(Val))
    {
        const char* RawStr = yyjson_get_str(Val);
        if (RawStr)
        {
            return FCString::Atoi64(UTF8_TO_TCHAR(RawStr));
        }
    }

    return DefaultValue;
}

double FYyJsonParser::GetNumber(yyjson_val* Val, double DefaultValue) const
{
    if (!Val) return DefaultValue;

    if (yyjson_is_num(Val))
    {
        return yyjson_get_num(Val);
    }

    if (yyjson_is_str(Val))
    {
        const char* RawStr = yyjson_get_str(Val);
        if (RawStr)
        {
            return FCString::Atod(UTF8_TO_TCHAR(RawStr));
        }
    }

    return DefaultValue;
}

bool FYyJsonParser::GetBool(yyjson_val* Val, bool DefaultValue) const
{
    if (Val && yyjson_is_bool(Val))
    {
        return yyjson_get_bool(Val);
    }
    return DefaultValue;
}

size_t FYyJsonParser::GetArraySize(yyjson_val* ArrayVal) const
{
    if (ArrayVal && yyjson_is_arr(ArrayVal))
    {
        return yyjson_arr_size(ArrayVal);
    }
    return 0;
}