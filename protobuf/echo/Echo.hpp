#ifndef PROTOBUF_ECHO_HPP
#define PROTOBUF_ECHO_HPP

#include "infra/util/Function.hpp"
#include "infra/util/Optional.hpp"
#include "infra/syntax/ProtoParser.hpp"
#include "services/network/Connection.hpp"
#include "services/network/MessageCommunication.hpp"

namespace services
{
    class Service;
    class ServiceProxy;

    class Echo
    {
    public:
        virtual void RequestSend(ServiceProxy& serviceProxy) = 0;
        virtual infra::StreamWriter& SendStreamWriter() = 0;
        virtual void Send() = 0;
        virtual void ServiceDone(Service& service) = 0;
        virtual void AttachService(Service& service) = 0;
        virtual void DetachService(Service& service) = 0;
    };

    class Service
        : public infra::IntrusiveList<Service>::NodeType
    {
    public:
        Service(Echo& echo, uint32_t id);
        Service(const Service& other) = delete;
        Service& operator=(const Service& other) = delete;
        ~Service();

        void MethodDone();
        uint32_t ServiceId() const;
        bool InProgress() const;
        void HandleMethod(uint32_t methodId, infra::ProtoParser& parser);

    protected:
        Echo& Rpc();
        virtual void Handle(uint32_t methodId, infra::ProtoParser& parser) = 0;

    private:
        Echo& echo;
        uint32_t serviceId;
        bool inProgress = false;
    };

    class ServiceProxy
        : public infra::IntrusiveList<ServiceProxy>::NodeType
    {
    public:
        ServiceProxy(Echo& echo, uint32_t id, uint32_t maxMessageSize);

        Echo& Rpc();
        void RequestSend(infra::Function<void()> onGranted);
        void GrantSend();
        uint32_t MaxMessageSize() const;

    private:
        Echo& echo;
        uint32_t serviceId;
        uint32_t maxMessageSize;
        infra::Function<void()> onGranted;
    };

    class EchoOnStreams
        : public Echo
        , public infra::EnableSharedFromThis<EchoOnStreams>
    {
    public:
        // Implementation of Echo
        virtual void RequestSend(ServiceProxy& serviceProxy) override;
        virtual infra::StreamWriter& SendStreamWriter() override;
        virtual void Send() override;
        virtual void ServiceDone(Service& service) override;
        virtual void AttachService(Service& service) override;
        virtual void DetachService(Service& service) override;

    protected:
        virtual void RequestSendStream(std::size_t size) = 0;
        virtual void ServiceDone() = 0;

        void ExecuteMethod(uint32_t serviceId, uint32_t methodId, infra::ProtoParser& argument);
        void SetStreamWriter(infra::SharedPtr<infra::StreamWriter>&& writer);
        bool ServiceBusy() const;

    private:
        infra::IntrusiveList<Service> services;
        infra::SharedPtr<infra::StreamWriter> streamWriter;
        infra::IntrusiveList<ServiceProxy> sendRequesters;
        infra::Optional<uint32_t> serviceBusy;
    };

    class EchoOnConnection
        : public EchoOnStreams
        , public ConnectionObserver
    {
    public:
        // Implementation of ConnectionObserver
        virtual void SendStreamAvailable(infra::SharedPtr<infra::StreamWriter>&& writer) override;
        virtual void DataReceived() override;

    protected:
        virtual void RequestSendStream(std::size_t size) override;
        virtual void ServiceDone() override;
    };

    class EchoOnMessageCommunication
        : public EchoOnStreams
        , public MessageCommunicationObserver
    {
    public:
        // Implementation of MessageCommunicationObserver
        virtual void SendMessageStreamAvailable(infra::SharedPtr<infra::StreamWriter>&& writer) override;
        virtual void ReceivedMessage(infra::SharedPtr<infra::StreamReaderWithRewinding>&& reader) override;

    protected:
        virtual void RequestSendStream(std::size_t size) override;
        virtual void ServiceDone() override;

    private:
        void ProcessMessage();

    private:
        infra::SharedPtr<infra::StreamReaderWithRewinding> reader;
    };
}

#endif
