int main() {
#pragma pack(1)
  struct WsFrameHeader {
    unsigned opcode : 4;
    unsigned rsv3 : 1;
    unsigned rsv2 : 1;
    unsigned rsv1 : 1;
    unsigned fin : 1;

    unsigned payload_prefix : 7;
    unsigned mask : 1;
  };
#pragma pack()

  unsigned char data[] = {0x81, 0x84};
  struct WsFrameHeader *header = (struct WsFrameHeader *)data;

  struct WsFrameHeader header2 {
    .opcode = 0x1, .fin = 1, .payload_prefix = 0x4, .mask = 0x1,
  };
  unsigned char *data2 = (unsigned char *)&header2;

  return 0;
}