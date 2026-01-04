#pragma once

#include "CoreMinimal.h"
#include "yyjson.h" // yyjson.h 파일이 프로젝트 include 경로에 있어야 합니다.

/**
 * yyjson 라이브러리를 언리얼 엔진에서 쉽게 사용하기 위한 래퍼 클래스입니다.
 * 문서의 수명(Lifecycle)을 관리하고 FString 변환을 처리합니다.
 */
class DTCORE_API FYyJsonParser
{
public:
    FYyJsonParser();
    ~FYyJsonParser();

    // 복사 방지 (yyjson_doc 포인터 소유권 문제 방지)
    FYyJsonParser(const FYyJsonParser&) = delete;
    FYyJsonParser& operator=(const FYyJsonParser&) = delete;

public:
    /**
     * JSON 문자열을 파싱하여 문서를 생성합니다.
     * @param JsonContent 파싱할 JSON 문자열
     * @return 파싱 성공 여부
     */
    bool JsonParse(const FString& JsonContent);

    /**
     * JSON Pointer 문법을 사용하여 값에 접근합니다. (예: "/users/0/name")
     * @param Path JSON Pointer 경로
     * @return 해당 경로의 yyjson_val 포인터 (없으면 nullptr)
     */
    yyjson_val* JsonParsePtr(const FString& Path) const;

    /**
     * 특정 키워드(Key)로 객체의 값을 찾습니다.
     * @param ObjectVal 검색할 부모 객체 (nullptr이면 Root에서 검색)
     * @param Key 찾을 키 문자열
     * @return 찾은 값의 yyjson_val 포인터
     */
    yyjson_val* JsonParseKeyword(yyjson_val* ObjectVal, const FString& Key) const;

    /**
     * 배열 노드인지 확인하고 반환합니다.
     * @param ArrayVal 확인할 노드
     * @return 배열이면 해당 노드, 아니면 nullptr
     */
    yyjson_val* JsonParseArr(yyjson_val* ArrayVal) const;

    /**
     * 배열의 특정 인덱스에 있는 값을 가져옵니다.
     * @param ArrayVal 배열 노드
     * @param Index 가져올 인덱스
     * @return 해당 인덱스의 yyjson_val 포인터
     */
    yyjson_val* JsonParseArrIndex(yyjson_val* ArrayVal, int32 Index) const;

    // --- 편의를 위한 Getter 함수들 ---

    /** 현재 문서의 Root 노드를 반환합니다. */
    yyjson_val* GetRoot() const { return Root; }

    /** 값이 유효한지 확인합니다. */
    bool IsValid(yyjson_val* Val) const { return Val != nullptr; }

    /** 값을 FString으로 변환합니다. */
    FString GetString(yyjson_val* Val, const FString& DefaultValue = TEXT("")) const;

    /** 값을 int32로 변환합니다. */
    int32 GetInt(yyjson_val* Val, int32 DefaultValue = 0) const;

    /** 값을 int64로 변환합니다. */
    int64 GetInt64(yyjson_val* Val, int64 DefaultValue = 0) const;

    /** 값을 float/double로 변환합니다. */
    double GetNumber(yyjson_val* Val, double DefaultValue = 0.0) const;

    /** 값을 bool로 변환합니다. */
    bool GetBool(yyjson_val* Val, bool DefaultValue = false) const;

    /** 배열의 크기를 반환합니다. */
    size_t GetArraySize(yyjson_val* ArrayVal) const;

private:
    yyjson_doc* Doc = nullptr;
    yyjson_val* Root = nullptr;
};