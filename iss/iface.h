#ifndef ISS_IFACE_H
#define ISS_IFACE_H 1

struct IReceiver {
  virtual void Receive(const char* data, unsigned int size) = 0;
  virtual ~IReceiver();
};

struct ICallback
{
    virtual void BinaryPacket(const char* data, unsigned int size) = 0;
    virtual void TextPacket(const char* data, unsigned int size) = 0;
    virtual ~ICallback();
};

#endif  /* ISS_IFACE_H */
