#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <memory>
#include <napi.h>
#include <memory>
#include <vector>

typedef uint64_t sequence_t;

class Disruptor : public Napi::ObjectWrap<Disruptor>
{
public:
    Disruptor(const Napi::CallbackInfo& info);
    ~Disruptor();

    static void Initialize(Napi::Env env, Napi::Object exports);

    // Unmap the shared memory. Don't access it again from this Disruptor!
    void Release(const Napi::CallbackInfo& info);

    // Return unconsumed values for a consumer
    Napi::Value ConsumeNew(const Napi::CallbackInfo& info); 
    Napi::Value ConsumeNewSync(const Napi::CallbackInfo& info); 

    // Update consumer sequence without consuming more
    void ConsumeCommit(const Napi::CallbackInfo&);

    // Claim a slot for writing a value
    Napi::Value ProduceClaim(const Napi::CallbackInfo& info);
    Napi::Value ProduceClaimSync(const Napi::CallbackInfo& info);

    // Commit a claimed slot.
    Napi::Value ProduceCommit(const Napi::CallbackInfo& info);
    Napi::Value ProduceCommitSync(const Napi::CallbackInfo& info);

    inline bool Spin()
    {
        return spin;
    }

private:
    friend class ConsumeNewAsyncWorker;
    friend class ProduceClaimAsyncWorker;
    friend class ProduceCommitAsyncWorker;

    int Release();

    void UpdatePending(sequence_t seq_consumer, sequence_t seq_cursor);

    template<typename Array, template<typename> typename Buffer>
    Array ConsumeNewSync(const Napi::Env& env, bool retry);
    void ConsumeNewAsync(const Napi::CallbackInfo& info); 

    void ConsumeCommit();

    template<template<typename> typename Buffer, typename Number>
    Buffer<uint8_t> ProduceClaimSync(const Napi::Env& env, bool retry);
    void ProduceClaimAsync(const Napi::CallbackInfo& info);

    template<typename Boolean>
    Boolean ProduceCommitSync(const Napi::Env& env, sequence_t seq_next, bool retry);
    void ProduceCommitAsync(const Napi::CallbackInfo& info);
    void ProduceCommitAsync(const Napi::CallbackInfo& info,
                            sequence_t seq_next);

    static sequence_t GetSeqNext(const Napi::CallbackInfo& info);

    uint32_t num_elements;
    uint32_t element_size;
    uint32_t num_consumers;
    uint32_t consumer;
    bool spin;

    size_t shm_size;
    void* shm_buf;
    
    sequence_t *consumers; // for each consumer, next slot to read
    sequence_t *cursor;    // next slot to be filled
    sequence_t *next;      // next slot to claim
    uint8_t* elements;
    sequence_t *ptr_consumer;

    sequence_t pending_seq_consumer;
    sequence_t pending_seq_cursor;

    Napi::Value GetConsumers(const Napi::CallbackInfo& info);
    Napi::Value GetCursor(const Napi::CallbackInfo& info);
    Napi::Value GetNext(const Napi::CallbackInfo& info);
    Napi::Value GetElements(const Napi::CallbackInfo& info);
    Napi::Value GetConsumer(const Napi::CallbackInfo& info);
};

void NullCallback(const Napi::CallbackInfo& info)
{
}

Napi::Function GetCallback(const Napi::CallbackInfo& info, uint32_t cb_arg)
{
    if (info.Length() > cb_arg)
    {
        Napi::Value cb = info[cb_arg];
        if (cb.IsFunction())
        {
            return cb.As<Napi::Function>();
        }
    }

    return Napi::Function::New(info.Env(), NullCallback);
}

template <typename Result>
class DisruptorAsyncWorker : public Napi::AsyncWorker
{
public:
    DisruptorAsyncWorker(Disruptor *disruptor,
                         const Napi::Function& callback) :
        Napi::AsyncWorker(callback),
        retry(false),
        disruptor(disruptor),
        disruptor_ref(Napi::Persistent(disruptor->Value()))
    {
    }

protected:
    virtual void Retry() = 0;

    void OnOK() override
    {
        if (disruptor->Spin() && retry)
        {
            return Retry();
        }

        Callback().MakeCallback(
            Receiver().Value(),
            std::initializer_list<napi_value>{Env().Null(), result.ToValue(Env())});
    }

    Result result;
    bool retry;
    Disruptor *disruptor;

private:
    Napi::ObjectReference disruptor_ref;
};

template<typename T>
struct AsyncBuffer
{
    static AsyncBuffer New(Napi::Env, T* data, size_t length)
    {
        return AsyncBuffer{data, length};
    }

    static AsyncBuffer New(Napi::Env env, size_t length)
    {
        return AsyncBuffer{nullptr, length};
    }

    void Set(const char* /* "seq_next" */, sequence_t value)
    {
        seq_next = value;
        seq_next_set = true;
    }

    Napi::Value ToValue(Napi::Env env)
    {
        Napi::Buffer<T> r;

        if (data)
        {
            r = Napi::Buffer<T>::New(env, data, length);
        }
        else
        {
            r = Napi::Buffer<T>::New(env, length);
        }

        if (seq_next_set)
        {
            r.Set("seq_next", Napi::Number::New(env, seq_next));
        }

        return r;
    }

    size_t Length()
    {
        return length;
    }

    T* data;
    size_t length;
    sequence_t seq_next = 0;
    bool seq_next_set = false;
};

template<typename T>
struct AsyncArray
{
    static AsyncArray New(Napi::Env)
    {
        return AsyncArray{std::make_unique<std::vector<T>>()};
    }

    void Set(uint32_t index, T el)
    {
        if (elements->size() <= index)
        {
            elements->resize(index + 1);
        }

        (*elements)[index] = el;
    }

    Napi::Value ToValue(Napi::Env env)
    {
        size_t length = elements->size();
        Napi::Array r = Napi::Array::New(env);
        for (size_t i = 0; i < length; ++i)
        {
            r[i] = (*elements)[i].ToValue(env);
        }
        return r;
    }

    size_t Length()
    {
        return elements->size();
    }

    std::unique_ptr<std::vector<T>> elements;
};

struct AsyncBoolean
{
    static AsyncBoolean New(Napi::Env, bool b)
    {
        return AsyncBoolean{b};
    }

    Napi::Value ToValue(Napi::Env env)
    {
        return Napi::Boolean::New(env, b);
    }

    operator bool() const
    {
        return b;
    }

    bool b;
};

struct AsyncNumber
{
    static sequence_t New(Napi::Env, sequence_t n)
    {
        return n;
    }
};

struct CloseFD
{
    void operator()(int *fd)
    {
        close(*fd);
        delete fd;
    }
};

void ThrowErrnoError(const Napi::CallbackInfo& info, const char *msg)
{
    int errnum = errno;
    char buf[1024] = {0};
    auto errmsg = strerror_r(errnum, buf, sizeof(buf));
    static_assert(std::is_same<decltype(errmsg), char*>::value,
                  "strerror_r must return char*");
    throw Napi::Error::New(info.Env(), 
        std::string(msg) + ": " + (errmsg ? errmsg : std::to_string(errnum)));
}

Disruptor::Disruptor(const Napi::CallbackInfo& info) :
    Napi::ObjectWrap<Disruptor>(info),
    shm_buf(MAP_FAILED)
{
    // Arguments
    Napi::String shm_name = info[0].As<Napi::String>();
    num_elements = info[1].As<Napi::Number>();
    element_size = info[2].As<Napi::Number>();
    num_consumers = info[3].As<Napi::Number>();
    consumer = info[4].As<Napi::Number>();
    bool init = info[5].As<Napi::Boolean>();
    spin = info[6].As<Napi::Boolean>();

    // Open shared memory object
    std::unique_ptr<int, CloseFD> shm_fd(new int(
        shm_open(shm_name.Utf8Value().c_str(),
                 O_CREAT | O_RDWR | (init ? O_TRUNC : 0),
                 S_IRUSR | S_IWUSR)));
    if (*shm_fd < 0)
    {
        ThrowErrnoError(info, "Failed to open shared memory object");
    }

    // Allow space for all the elements,
    // a sequence number for each consumer,
    // the cursor sequence number (last filled slot) and
    // the next sequence number (first free slot).
    shm_size = (num_consumers + 2) * sizeof(sequence_t) +
               num_elements * element_size;

    // Resize the shared memory if we're initializing it.
    // Note: ftruncate initializes to null bytes.
    if (init && (ftruncate(*shm_fd, shm_size) < 0))
    {
        ThrowErrnoError(info, "Failed to size shared memory"); //LCOV_EXCL_LINE
    }

    // Map the shared memory
    shm_buf = mmap(NULL,
                   shm_size,
                   PROT_READ | PROT_WRITE, MAP_SHARED,
                   *shm_fd,
                   0);
    if (shm_buf == MAP_FAILED)
    {
        ThrowErrnoError(info, "Failed to map shared memory"); //LCOV_EXCL_LINE

    }

    consumers = static_cast<sequence_t*>(shm_buf);
    cursor = &consumers[num_consumers];
    next = &cursor[1];
    elements = reinterpret_cast<uint8_t*>(&next[1]);
    ptr_consumer = &consumers[consumer];

    pending_seq_consumer = 0;
    pending_seq_cursor = 0;
}

Disruptor::~Disruptor()
{
    Release();
}

int Disruptor::Release()
{
    if (shm_buf != MAP_FAILED)
    {
        int r = munmap(shm_buf, shm_size);

        if (r < 0)
        {
            return r;
        }

        shm_buf = MAP_FAILED;
    }

    return 0;
}

void Disruptor::Release(const Napi::CallbackInfo& info)
{
    if (Release() < 0)
    {
        ThrowErrnoError(info, "Failed to unmap shared memory"); //LCOV_EXCL_LINE

    }
}

template<typename Array, template<typename> typename Buffer>
Array Disruptor::ConsumeNewSync(const Napi::Env& env, bool retry)
{
    // Return all elements [&consumers[consumer], cursor)

    // Commit previous consume
    ConsumeCommit();

    Array r = Array::New(env);
    sequence_t seq_consumer, pos_consumer;
    sequence_t seq_cursor, pos_cursor;

    do
    {
        seq_consumer = __sync_val_compare_and_swap(ptr_consumer, 0, 0);
        seq_cursor = __sync_val_compare_and_swap(cursor, 0, 0);
        pos_consumer = seq_consumer % num_elements;
        pos_cursor = seq_cursor % num_elements;

        if (pos_cursor > pos_consumer)
        {
            r.Set(0U, Buffer<uint8_t>::New(
                env,
                elements + pos_consumer * element_size,
                (pos_cursor - pos_consumer) * element_size));

            UpdatePending(seq_consumer, seq_cursor);
            break;
        }

        if (seq_cursor != seq_consumer)
        {
            r.Set(0U, Buffer<uint8_t>::New(
                env,
                elements + pos_consumer * element_size,
                (num_elements - pos_consumer) * element_size));

            if (pos_cursor > 0)
            {
                r.Set(1U, Buffer<uint8_t>::New(
                    env,
                    elements,
                    pos_cursor * element_size));
            }

            UpdatePending(seq_consumer, seq_cursor);
            break;
        }

        if (!retry)
        {
            break;
        }
    }
    while (true);

    return r;
}

Napi::Value Disruptor::ConsumeNewSync(const Napi::CallbackInfo& info)
{
    return ConsumeNewSync<Napi::Array, Napi::Buffer>(info.Env(), spin);
}

class ConsumeNewAsyncWorker :
    public DisruptorAsyncWorker<AsyncArray<AsyncBuffer<uint8_t>>>
{
public:
    ConsumeNewAsyncWorker(Disruptor *disruptor,
                          const Napi::Function& callback) :
        DisruptorAsyncWorker<AsyncArray<AsyncBuffer<uint8_t>>>(
            disruptor, callback)
    {
    }

protected:
    void Execute() override
    {
        // Remember: don't access any V8 stuff in worker thread
        result = disruptor->ConsumeNewSync<AsyncArray<AsyncBuffer<uint8_t>>, AsyncBuffer>(Env(), false);
        retry = result.Length() == 0;
    }

    void Retry() override
    {
        (new ConsumeNewAsyncWorker(disruptor, Callback().Value()))->Queue();
    }
};

void Disruptor::ConsumeNewAsync(const Napi::CallbackInfo& info)
{
    (new ConsumeNewAsyncWorker(this, GetCallback(info, 0)))->Queue();
}

Napi::Value Disruptor::ConsumeNew(const Napi::CallbackInfo& info)
{
    Napi::Array r = ConsumeNewSync<Napi::Array, Napi::Buffer>(info.Env(), false);
    if (r.Length() > 0)
    {
        return r;
    }

    ConsumeNewAsync(info);
    return info.Env().Undefined();
}

void Disruptor::ConsumeCommit(const Napi::CallbackInfo&)
{
    ConsumeCommit();
}

void Disruptor::UpdatePending(sequence_t seq_consumer, sequence_t seq_cursor)
{
    pending_seq_consumer = seq_consumer;
    pending_seq_cursor = seq_cursor;
}

void Disruptor::ConsumeCommit()
{
    if (pending_seq_cursor)
    {
        __sync_val_compare_and_swap(ptr_consumer,
                                    pending_seq_consumer,
                                    pending_seq_cursor);
        pending_seq_consumer = 0;
        pending_seq_cursor = 0;
    }
}

template<template<typename> typename Buffer, typename Number>
Buffer<uint8_t> Disruptor::ProduceClaimSync(const Napi::Env& env, bool retry)
{
    sequence_t seq_next, pos_next;
    sequence_t seq_consumer, pos_consumer;
    bool can_claim;

    do
    {
        seq_next = __sync_val_compare_and_swap(next, 0, 0);
        pos_next = seq_next % num_elements;

        can_claim = true;

        for (uint32_t i = 0; i < num_consumers; ++i)
        {
            seq_consumer = __sync_val_compare_and_swap(&consumers[i], 0, 0);
            pos_consumer = seq_consumer % num_elements;

            if ((pos_consumer == pos_next) && (seq_consumer != seq_next))
            {
                can_claim = false;
                break;
            }
        }

        if (can_claim &&
            __sync_bool_compare_and_swap(next, seq_next, seq_next + 1))
        {
            Buffer<uint8_t> r = Buffer<uint8_t>::New(
                env,
                elements + pos_next * element_size,
                element_size);

            r.Set("seq_next", Number::New(env, seq_next));

            return r;
        }

        if (!retry)
        {
            return Buffer<uint8_t>::New(env, 0);
        }
    }
    while (true);
}

Napi::Value Disruptor::ProduceClaimSync(const Napi::CallbackInfo& info)
{
    return ProduceClaimSync<Napi::Buffer, Napi::Number>(info.Env(), spin);
}

class ProduceClaimAsyncWorker :
    public DisruptorAsyncWorker<AsyncBuffer<uint8_t>>
{
public:
    ProduceClaimAsyncWorker(Disruptor *disruptor,
                            const Napi::Function& callback) :
        DisruptorAsyncWorker<AsyncBuffer<uint8_t>>(disruptor, callback)
    {
    }

protected:
    void Execute() override
    {
        // Remember: don't access any V8 stuff in worker thread
        result = disruptor->ProduceClaimSync<AsyncBuffer, AsyncNumber>(Env(), false);
        retry = result.Length() == 0;
    }

    void Retry() override
    {
        (new ProduceClaimAsyncWorker(disruptor, Callback().Value()))->Queue();
    }
};

void Disruptor::ProduceClaimAsync(const Napi::CallbackInfo& info)
{
    (new ProduceClaimAsyncWorker(this, GetCallback(info, 0)))->Queue();
}

Napi::Value Disruptor::ProduceClaim(const Napi::CallbackInfo& info)
{
    Napi::Buffer<uint8_t> r = ProduceClaimSync<Napi::Buffer, Napi::Number>(info.Env(), false);
    if (r.Length() > 0)
    {
        return r;
    }

    ProduceClaimAsync(info);
    return info.Env().Undefined();
}

template<typename Boolean>
Boolean Disruptor::ProduceCommitSync(const Napi::Env& env,
                                     sequence_t seq_next,
                                     bool retry)
{
    do
    {
        if (__sync_bool_compare_and_swap(cursor, seq_next, seq_next + 1))
        {
            return Boolean::New(env, true);
        }

        if (!retry)
        {
            return Boolean::New(env, false);
        }
    }
    while (true);
}

sequence_t Disruptor::GetSeqNext(const Napi::CallbackInfo& info)
{
    return info[0].As<Napi::Object>().Get("seq_next").As<Napi::Number>().Int64Value();
}

Napi::Value Disruptor::ProduceCommitSync(const Napi::CallbackInfo& info)
{
    return ProduceCommitSync<Napi::Boolean>(info.Env(), GetSeqNext(info), spin);
}

class ProduceCommitAsyncWorker :
    public DisruptorAsyncWorker<AsyncBoolean>
{
public:
    ProduceCommitAsyncWorker(Disruptor *disruptor,
                             const Napi::Function& callback,
                             sequence_t seq_next) :
        DisruptorAsyncWorker<AsyncBoolean>(disruptor, callback),
        seq_next(seq_next)
    {
    }

protected:
    void Execute() override
    {
        // Remember: don't access any V8 stuff in worker thread
        result = disruptor->ProduceCommitSync<AsyncBoolean>(Env(), seq_next, false);
        retry = !result;
    }

    void Retry() override
    {
        (new ProduceCommitAsyncWorker(disruptor, Callback().Value(), seq_next))->Queue();
    }

private:
    sequence_t seq_next;
};

void Disruptor::ProduceCommitAsync(const Napi::CallbackInfo& info,
                                   sequence_t seq_next)
{
    (new ProduceCommitAsyncWorker(this, GetCallback(info, 1), seq_next))->Queue();
}

void Disruptor::ProduceCommitAsync(const Napi::CallbackInfo& info)
{
    ProduceCommitAsync(info, Disruptor::GetSeqNext(info));
}

Napi::Value Disruptor::ProduceCommit(const Napi::CallbackInfo& info)
{
    sequence_t seq_next = Disruptor::GetSeqNext(info);

    Napi::Boolean r = ProduceCommitSync<Napi::Boolean>(info.Env(), seq_next, false);
    if (r)
    {
        return r;
    }

    ProduceCommitAsync(info, seq_next);
    return info.Env().Undefined();
}

Napi::Value Disruptor::GetConsumers(const Napi::CallbackInfo& info)
{
    return Napi::Buffer<sequence_t>::New(info.Env(), consumers, num_consumers);
}

Napi::Value Disruptor::GetCursor(const Napi::CallbackInfo& info)
{
    return Napi::Number::New(info.Env(), __sync_val_compare_and_swap(cursor, 0, 0));
}

Napi::Value Disruptor::GetNext(const Napi::CallbackInfo& info)
{
    return Napi::Number::New(info.Env(), __sync_val_compare_and_swap(next, 0, 0));
}

Napi::Value Disruptor::GetElements(const Napi::CallbackInfo& info)
{
    return Napi::Buffer<uint8_t>::New(info.Env(), elements, num_elements * element_size);
}

Napi::Value Disruptor::GetConsumer(const Napi::CallbackInfo& info)
{
    return Napi::Number::New(info.Env(), __sync_val_compare_and_swap(ptr_consumer, 0, 0));
}

void Disruptor::Initialize(Napi::Env env, Napi::Object exports)
{
    exports.Set("Disruptor", DefineClass(env, "Disruptor",
    {
        InstanceMethod("release", &Disruptor::Release),
        InstanceMethod("consumeNew", &Disruptor::ConsumeNew),
        InstanceMethod("consumeNewSync", &Disruptor::ConsumeNewSync),
        InstanceMethod("consumeCommit", &Disruptor::ConsumeCommit),
        InstanceMethod("produceClaim", &Disruptor::ProduceClaim),
        InstanceMethod("produceClaimSync", &Disruptor::ProduceClaimSync),
        InstanceMethod("produceCommit", &Disruptor::ProduceCommit),
        InstanceMethod("produceCommitSync", &Disruptor::ProduceCommitSync),

        // For testing only
        InstanceAccessor("consumers", &Disruptor::GetConsumers, nullptr),
        InstanceAccessor("cursor", &Disruptor::GetCursor, nullptr),
        InstanceAccessor("next", &Disruptor::GetNext, nullptr),
        InstanceAccessor("elements", &Disruptor::GetElements, nullptr),
        InstanceAccessor("consumer", &Disruptor::GetConsumer, nullptr),
        InstanceMethod("consumeNewAsync", &Disruptor::ConsumeNewAsync),
        InstanceMethod("produceClaimAsync", &Disruptor::ProduceClaimAsync),
        InstanceMethod("produceCommitAsync", &Disruptor::ProduceCommitAsync),
    }));
}

void Initialize(Napi::Env env, Napi::Object exports, Napi::Object module)
{
    Disruptor::Initialize(env, exports);
}

NODE_API_MODULE(disruptor, Initialize)
