#if WITH_DEV_AUTOMATION_TESTS

#include "HttpModule.h"
#include "HttpManager.h"
#include "HttpPath.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Containers/StringConv.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorDataTransportComp.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorTransportHttpPostLoopbackTest, "MA0T10.SensorTransport.HttpPostLoopbackAcceptance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
FString GetFirstHeaderValue(const FHttpServerRequest& Request, const FString& HeaderName)
{
    const TArray<FString>* Values = Request.Headers.Find(HeaderName);
    if (!Values || Values->Num() <= 0)
    {
        return FString();
    }
    return (*Values)[0];
}

int32 FindFreeLoopbackTcpPort()
{
    FSocket* ProbeSocket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("MA0T10_TransportHttpProbe"), false);
    if (!ProbeSocket)
    {
        return 0;
    }

    int32 FreePort = 0;
    TSharedRef<FInternetAddr> Address = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
    Address->SetIp(FIPv4Address(127, 0, 0, 1).Value);
    Address->SetPort(0);

    if (ProbeSocket->Bind(*Address))
    {
        TSharedRef<FInternetAddr> BoundAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
        ProbeSocket->GetAddress(*BoundAddress);
        FreePort = BoundAddress->GetPort();
    }

    ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ProbeSocket);
    return FreePort;
}
}

class FTransportHttpPostLoopbackCommand : public IAutomationLatentCommand
{
public:
    explicit FTransportHttpPostLoopbackCommand(FAutomationTestBase* InTest)
        : Test(InTest)
    {
    }

    virtual bool Update() override
    {
        if (!Test)
        {
            return true;
        }

        FHttpModule::Get().GetHttpManager().Tick(0.0f);

        if (StartTimeSeconds <= 0.0)
        {
            StartTimeSeconds = FPlatformTime::Seconds();
            return Setup();
        }

        if (Phase == EPhase::SendAccept)
        {
            return SendAcceptRequest();
        }

        if (Phase == EPhase::WaitAccept)
        {
            if (Transport.IsValid() && Transport->LastResult.HttpStatusCode == 202)
            {
                const FVirtualSensorTransportResult& Result = Transport->LastResult;
                Test->TestTrue(TEXT("HTTP 202 callback submitted"), Result.bSubmitted);
                Test->TestTrue(TEXT("HTTP 202 callback accepted"), Result.bAccepted);
                Test->TestEqual(TEXT("HTTP 202 status code"), Result.HttpStatusCode, 202);
                Test->TestTrue(TEXT("HTTP 202 response body captured"), Result.ResponseBody.Contains(TEXT("\"accepted\":true")));
                Test->TestTrue(TEXT("HTTP 202 response contract captured"), Result.ResponseBody.Contains(TEXT("mock-judging-server.v1")));
                Test->TestEqual(TEXT("HTTP 202 data length preserved"), Result.DataLength, AcceptPayload.Len());
                Test->TestEqual(TEXT("HTTP in-flight count returns to zero"), Transport->InFlightHttpRequestCount, 0);
                Test->TestEqual(TEXT("HTTP backpressure rejection counted"), Transport->BackpressureRejectedRequestCount, 1);
                Phase = EPhase::SendReject;
                return false;
            }
        }

        if (Phase == EPhase::SendReject)
        {
            return SendRejectRequest();
        }

        if (Phase == EPhase::WaitReject)
        {
            if (Transport.IsValid() && Transport->LastResult.HttpStatusCode == 400)
            {
                const FVirtualSensorTransportResult& Result = Transport->LastResult;
                Test->TestTrue(TEXT("HTTP 400 callback submitted"), Result.bSubmitted);
                Test->TestFalse(TEXT("HTTP 400 callback not accepted"), Result.bAccepted);
                Test->TestEqual(TEXT("HTTP 400 status code"), Result.HttpStatusCode, 400);
                Test->TestEqual(TEXT("HTTP 400 is not retried"), Result.RetryAttemptCount, 0);
                Test->TestFalse(TEXT("HTTP 400 does not report retry exhaustion"), Result.bRetryExhausted);
                Test->TestTrue(TEXT("HTTP 400 response body captured"), Result.ResponseBody.Contains(TEXT("\"accepted\":false")));
                Test->TestEqual(TEXT("HTTP 400 data length preserved"), Result.DataLength, RejectPayload.Len());
                Phase = EPhase::SendRetry;
                return false;
            }
        }

        if (Phase == EPhase::SendRetry)
        {
            return SendRetryRequest();
        }

        if (Phase == EPhase::WaitRetry)
        {
            if (Transport.IsValid() && Transport->LastResult.HttpStatusCode == 202)
            {
                const FVirtualSensorTransportResult& Result = Transport->LastResult;
                Test->TestTrue(TEXT("HTTP 503 retry eventually submitted"), Result.bSubmitted);
                Test->TestTrue(TEXT("HTTP 503 retry eventually accepted"), Result.bAccepted);
                Test->TestEqual(TEXT("HTTP retry final status code"), Result.HttpStatusCode, 202);
                Test->TestEqual(TEXT("HTTP retry attempt count"), Result.RetryAttemptCount, 1);
                Test->TestEqual(TEXT("HTTP cumulative retry count"), Transport->TotalHttpRetryAttemptCount, 1);
                Test->TestEqual(TEXT("HTTP retry in-flight count returns to zero"), Transport->InFlightHttpRequestCount, 0);
                Phase = EPhase::SendExhaust;
                return false;
            }
        }

        if (Phase == EPhase::SendExhaust)
        {
            return SendExhaustRequest();
        }

        if (Phase == EPhase::WaitExhaust)
        {
            if (Transport.IsValid() && Transport->LastResult.HttpStatusCode == 503 &&
                Transport->LastResult.bRetryExhausted)
            {
                const FVirtualSensorTransportResult& Result = Transport->LastResult;
                Test->TestFalse(TEXT("persistent HTTP 503 is not accepted"), Result.bAccepted);
                Test->TestTrue(TEXT("persistent HTTP 503 reports retry exhaustion"), Result.bRetryExhausted);
                Test->TestEqual(TEXT("persistent HTTP 503 uses one retry"), Result.RetryAttemptCount, 1);
                Test->TestEqual(TEXT("HTTP cumulative retry count includes exhausted request"), Transport->TotalHttpRetryAttemptCount, 2);
                Test->TestEqual(TEXT("HTTP failed request count includes 400 and exhausted 503"), Transport->FailedHttpRequestCount, 2);
                Test->TestEqual(TEXT("HTTP retry exhausted request count"), Transport->RetryExhaustedRequestCount, 1);
                Test->TestEqual(TEXT("loopback server received six requests"), ReceivedRequestCount, 6);
                Test->TestEqual(TEXT("loopback server received two retry-success attempts"), RetryRequestCount, 2);
                Test->TestEqual(TEXT("loopback server received two exhaustion attempts"), ExhaustRequestCount, 2);
                Test->TestEqual(TEXT("loopback server validated all request header sets"), ValidHeaderRequestCount, 6);
                Test->TestEqual(TEXT("loopback server validated all schema identities"), ValidSchemaRequestCount, 6);
                Cleanup();
                return true;
            }
        }

        if ((FPlatformTime::Seconds() - StartTimeSeconds) > TimeoutSeconds)
        {
            Test->AddError(FString::Printf(TEXT("HTTP transport loopback timed out. phase=%d status=%d requests=%d"),
                static_cast<int32>(Phase),
                Transport.IsValid() ? Transport->LastResult.HttpStatusCode : -1,
                ReceivedRequestCount));
            Cleanup();
            return true;
        }

        return false;
    }

private:
    enum class EPhase : uint8
    {
        Setup,
        SendAccept,
        WaitAccept,
        SendReject,
        WaitReject,
        SendRetry,
        WaitRetry,
        SendExhaust,
        WaitExhaust
    };

    bool Setup()
    {
        const int32 FreePort = FindFreeLoopbackTcpPort();
        Test->TestTrue(TEXT("transport loopback found a free TCP port"), FreePort > 0);
        if (FreePort <= 0)
        {
            return true;
        }

        RoutePath = FString::Printf(TEXT("/ma0t10/transport/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
        Router = FHttpServerModule::Get().GetHttpRouter(static_cast<uint32>(FreePort), true);
        Test->TestTrue(TEXT("transport loopback router created"), Router.IsValid());
        if (!Router.IsValid())
        {
            return true;
        }

        RouteHandle = Router->BindRoute(
            FHttpPath(RoutePath),
            EHttpServerRequestVerbs::VERB_POST,
            [this](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
            {
                ++ReceivedRequestCount;
                const FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
                const FString RequestBody(Converter.Length(), Converter.Get());
                const bool bAccept = RequestBody.Contains(TEXT("\"accept\":true"));
                const bool bRetryRequest = RequestBody.Contains(TEXT("\"retry\":true"));
                const bool bExhaustRequest = RequestBody.Contains(TEXT("\"exhaust\":true"));
                if (bRetryRequest)
                {
                    ++RetryRequestCount;
                }
                if (bExhaustRequest)
                {
                    ++ExhaustRequestCount;
                }
                const bool bHeadersValid =
                    GetFirstHeaderValue(Request, TEXT("Content-Type")).Contains(TEXT("application/json")) &&
                    GetFirstHeaderValue(Request, TEXT("X-Sensor-Id")) == TEXT("TEST-LIDAR-HTTP-TRANSPORT") &&
                    GetFirstHeaderValue(Request, TEXT("X-Sensor-Type")) == TEXT("virtual_lidar");
                const bool bSchemaValid = RequestBody.Contains(TEXT("\"schemaVersion\":\"virtual-lidar.v1\""));
                if (bHeadersValid)
                {
                    ++ValidHeaderRequestCount;
                }
                if (bSchemaValid)
                {
                    ++ValidSchemaRequestCount;
                }
                const bool bRetryServerFailure =
                    (bRetryRequest && RetryRequestCount == 1) ||
                    bExhaustRequest;
                const bool bAcceptedByMockJudge = bAccept && !bRetryServerFailure && bHeadersValid && bSchemaValid;
                const FString ResponseJson = bRetryServerFailure
                    ? TEXT("{\"accepted\":false,\"message\":\"temporary server failure\"}")
                    : (bAccept
                        ? TEXT("{\"accepted\":true,\"contract\":\"mock-judging-server.v1\",\"message\":\"accepted by loopback\"}")
                        : TEXT("{\"accepted\":false,\"message\":\"rejected by loopback\"}"));
                TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseJson, TEXT("application/json"));
                Response->Code = bRetryServerFailure
                    ? static_cast<EHttpServerResponseCodes>(503)
                    : (bAcceptedByMockJudge ? EHttpServerResponseCodes::Accepted : EHttpServerResponseCodes::BadRequest);
                OnComplete(MoveTemp(Response));
                return true;
            });

        Test->TestTrue(TEXT("transport loopback route bound"), RouteHandle.IsValid());
        if (!RouteHandle.IsValid())
        {
            Cleanup();
            return true;
        }

        FHttpServerModule::Get().StartAllListeners();

        Transport = NewObject<UVirtualSensorDataTransportComp>();
        Test->TestNotNull(TEXT("transport component"), Transport.Get());
        if (!Transport.IsValid())
        {
            Cleanup();
            return true;
        }
        Transport->AddToRoot();

        Transport->TransportMode = EVirtualSensorTransportMode::HttpPost;
        Transport->HttpEndpoint = FString::Printf(TEXT("http://127.0.0.1:%d%s"), FreePort, *RoutePath);
        Transport->HttpTimeoutSeconds = 5;
        Transport->MaxInFlightHttpRequests = 1;
        Transport->MaxHttpRetryAttempts = 1;
        Transport->bRetryOnConnectionFailure = true;
        Transport->bRetryOnServerError = true;
        Transport->bLogHttpResponse = false;
        Phase = EPhase::SendAccept;
        return false;
    }

    bool SendAcceptRequest()
    {
        AcceptPayload = TEXT("{\"accept\":true,\"schemaVersion\":\"virtual-lidar.v1\"}");
        const FVirtualSensorTransportResult InitialResult = Transport->SendJson(TEXT("TEST-LIDAR-HTTP-TRANSPORT"), TEXT("virtual_lidar"), AcceptPayload);
        Test->TestTrue(TEXT("HTTP 202 request submitted immediately"), InitialResult.bSubmitted);
        Test->TestFalse(TEXT("HTTP 202 request not accepted before callback"), InitialResult.bAccepted);
        Test->TestEqual(TEXT("HTTP 202 initial data length"), InitialResult.DataLength, AcceptPayload.Len());
        Test->TestEqual(TEXT("HTTP first request counted in flight"), Transport->InFlightHttpRequestCount, 1);

        const FVirtualSensorTransportResult BackpressureResult = Transport->SendJson(
            TEXT("TEST-LIDAR-HTTP-TRANSPORT"),
            TEXT("virtual_lidar"),
            AcceptPayload);
        Test->TestFalse(TEXT("HTTP concurrent request not submitted"), BackpressureResult.bSubmitted);
        Test->TestFalse(TEXT("HTTP concurrent request not accepted"), BackpressureResult.bAccepted);
        Test->TestTrue(TEXT("HTTP concurrent request marked backpressure rejected"), BackpressureResult.bBackpressureRejected);
        Test->TestTrue(TEXT("HTTP backpressure result includes limits"), BackpressureResult.Message.Contains(TEXT("inFlight=1 max=1")));
        Phase = EPhase::WaitAccept;
        return false;
    }

    bool SendRejectRequest()
    {
        RejectPayload = TEXT("{\"accept\":false,\"schemaVersion\":\"virtual-lidar.v1\"}");
        const FVirtualSensorTransportResult InitialResult = Transport->SendJson(TEXT("TEST-LIDAR-HTTP-TRANSPORT"), TEXT("virtual_lidar"), RejectPayload);
        Test->TestTrue(TEXT("HTTP 400 request submitted immediately"), InitialResult.bSubmitted);
        Test->TestFalse(TEXT("HTTP 400 request not accepted before callback"), InitialResult.bAccepted);
        Test->TestEqual(TEXT("HTTP 400 initial data length"), InitialResult.DataLength, RejectPayload.Len());
        Phase = EPhase::WaitReject;
        return false;
    }

    bool SendRetryRequest()
    {
        RetryPayload = TEXT("{\"accept\":true,\"retry\":true,\"schemaVersion\":\"virtual-lidar.v1\"}");
        const FVirtualSensorTransportResult InitialResult = Transport->SendJson(TEXT("TEST-LIDAR-HTTP-TRANSPORT"), TEXT("virtual_lidar"), RetryPayload);
        Test->TestTrue(TEXT("HTTP retry request submitted immediately"), InitialResult.bSubmitted);
        Test->TestFalse(TEXT("HTTP retry request not accepted before callback"), InitialResult.bAccepted);
        Phase = EPhase::WaitRetry;
        return false;
    }

    bool SendExhaustRequest()
    {
        ExhaustPayload = TEXT("{\"accept\":true,\"exhaust\":true,\"schemaVersion\":\"virtual-lidar.v1\"}");
        const FVirtualSensorTransportResult InitialResult = Transport->SendJson(TEXT("TEST-LIDAR-HTTP-TRANSPORT"), TEXT("virtual_lidar"), ExhaustPayload);
        Test->TestTrue(TEXT("HTTP exhaustion request submitted immediately"), InitialResult.bSubmitted);
        Test->TestFalse(TEXT("HTTP exhaustion request not accepted before callback"), InitialResult.bAccepted);
        Phase = EPhase::WaitExhaust;
        return false;
    }

    void Cleanup()
    {
        if (Router.IsValid() && RouteHandle.IsValid())
        {
            Router->UnbindRoute(RouteHandle);
        }
        RouteHandle.Reset();
        Router.Reset();
        if (Transport.IsValid())
        {
            Transport->RemoveFromRoot();
        }
        Transport.Reset();
    }

private:
    FAutomationTestBase* Test = nullptr;
    double StartTimeSeconds = 0.0;
    static constexpr double TimeoutSeconds = 8.0;
    EPhase Phase = EPhase::Setup;
    int32 ReceivedRequestCount = 0;
    int32 ValidHeaderRequestCount = 0;
    int32 ValidSchemaRequestCount = 0;
    int32 RetryRequestCount = 0;
    int32 ExhaustRequestCount = 0;
    FString RoutePath;
    FString AcceptPayload;
    FString RejectPayload;
    FString RetryPayload;
    FString ExhaustPayload;
    TSharedPtr<IHttpRouter> Router;
    FHttpRouteHandle RouteHandle;
    TWeakObjectPtr<UVirtualSensorDataTransportComp> Transport;
};

bool FVirtualSensorTransportHttpPostLoopbackTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FTransportHttpPostLoopbackCommand(this));
    return true;
}

#endif
