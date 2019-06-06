// See MultiProcessor Specification Version 1.[14]
// https://pdos.csail.mit.edu/6.828/2018/readings/ia32/MPspec.pdf

// MP Floating Pointer Structure
struct mp {
  uchar signature[4];           // シグネチャ("_MP_")
  void *physaddr;               // MPコンフィグテーブルの物理アドレス
  uchar length;                 // 長さ(1)
  uchar specrev;                // [14]
  uchar checksum;               // 全てのバイトを足し込んだ結果が0になるよう調整するためのもの？
  uchar type;                   // MP system config type
  uchar imcrp;                  // IMCR Pointer
  uchar reserved[3];
};

// MP configuration table header
struct mpconf {
  uchar signature[4];           // シグネチャ("PCMP")
  ushort length;                // テーブルサイズ
  uchar version;                // [14]
  uchar checksum;               // 全てのバイトを足し込んだ結果が0になるよう調整するためのもの？
  uchar product[20];            // product id
  uint *oemtable;               // OEM table pointer
  ushort oemlength;             // OEM table length
  ushort entry;                 // entry count
  uint *lapicaddr;              // ローカルAPICのアドレス
  ushort xlength;               // extended table length
  uchar xchecksum;              // extended table checksum
  uchar reserved;
};

// processor table entry
struct mpproc {
  uchar type;                   // エントリタイプ(0)
  uchar apicid;                 // ローカルAPICID
  uchar version;                // ローカルAPICバージョン
  uchar flags;                  // CPU flags
    #define MPBOOT 0x02           // 当プロセッサはブートストラッププロセッサ
  uchar signature[4];           // CPUシグネチャ
  uint feature;                 // feature flags from CPUID instruction
  uchar reserved[8];
};

struct mpioapic {       // I/O APIC table entry
  uchar type;                   // entry type (2)
  uchar apicno;                 // I/O APIC id
  uchar version;                // I/O APIC version
  uchar flags;                  // I/O APIC flags
  uint *addr;                  // I/O APIC address
};

// テーブルエントリタイプ
#define MPPROC    0x00  // 各プロセッサ毎に
#define MPBUS     0x01  // 各バス毎
#define MPIOAPIC  0x02  // 各I/O APIC毎
#define MPIOINTR  0x03  // 各バス割り込みソース
#define MPLINTR   0x04  // 各システム割り込みソース

//PAGEBREAK!
// Blank page.
