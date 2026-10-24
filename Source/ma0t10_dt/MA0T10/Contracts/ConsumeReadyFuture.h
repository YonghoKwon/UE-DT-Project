#pragma once
#include "Async/Future.h"

namespace MA0T10
{
/** UE5.3 has no TFuture::Consume. Consume an exclusively owned, ready future
 * without Get()'s by-value copy. The caller must not share its state or install
 * a continuation. Moving the future invalidates it before taking the result.
 */
template<typename ResultType>
ResultType ConsumeReadyFuture(TFuture<ResultType>& Future)
{
    check(Future.IsValid() && Future.IsReady());
    class FConsumer final : private TFuture<ResultType>
    {
    public:
        explicit FConsumer(TFuture<ResultType>&& Source) : TFuture<ResultType>(MoveTemp(Source)) {}
        ResultType Take()
        {
            // TFutureState stores a mutable result; UE5.3 exposes only a const
            // accessor. Exclusive ownership and completion make this one move
            // safe. Never apply this to TSharedFuture or a borrowed reference.
            return MoveTemp(const_cast<ResultType&>(this->GetState()->GetResult()));
        }
    };
    FConsumer Consumer(MoveTemp(Future));
    return Consumer.Take();
}
}
