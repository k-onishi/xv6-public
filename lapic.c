// The local APIC manages internal (non-I/O) interrupts.
// See Chapter 8 & Appendix C of Intel processor manual volume 3.

#include "param.h"
#include "types.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "traps.h"
#include "mmu.h"
#include "x86.h"

// ローカルAPICレジスタがマッピングされているアドレスからのインデックス。
// uint(4byte)配列のインデックスとして使用するため4で割る必要がある。
#define ID      (0x0020/4)   // ID
#define VER     (0x0030/4)   // Version
#define TPR     (0x0080/4)   // Task Priority
#define EOI     (0x00B0/4)   // EOI
#define SVR     (0x00F0/4)   // 仮の割り込みベクタ
  #define ENABLE     0x00000100   // Unit Enable
#define ESR     (0x0280/4)   // エラーステータス
#define ICRLO   (0x0300/4)   // 割り込みコマンド
  #define INIT       0x00000500   // 初期化/リセット
  #define STARTUP    0x00000600   // IPI(Inter-Processor Interrupt)の起動
  #define DELIVS     0x00001000   // Delivery status
  #define ASSERT     0x00004000   // Assert interrupt (vs deassert)
  #define DEASSERT   0x00000000
  #define LEVEL      0x00008000   // Level triggered
  #define BCAST      0x00080000   // Send to all APICs, including self.
  #define BUSY       0x00001000
  #define FIXED      0x00000000
#define ICRHI   (0x0310/4)   // 割り込みコマンド [63:32]
#define TIMER   (0x0320/4)   // Local Vector Table 0 (TIMER)
  #define X1         0x0000000B   // カウンタを1で割る
  #define PERIODIC   0x00020000   // 定期的に
#define PCINT   (0x0340/4)   // パフォーマンスカウンタ LVT
#define LINT0   (0x0350/4)   // ローカルベクタテーブル1(LINT0)
#define LINT1   (0x0360/4)   // ローカルベクタテーブル2(LINT1)
#define ERROR   (0x0370/4)   // ローカルベクタテーブル3(ERROR)
  #define MASKED     0x00010000   // マスクされた割り込み
#define TICR    (0x0380/4)   // タイマカウンタの初期値
#define TCCR    (0x0390/4)   // タイマカウンタの現在の値
#define TDCR    (0x03E0/4)   // タイマカウンタを除算設定

volatile uint *lapic;  // ローカルAPICのアドレス(mp.cで初期化される)

// ローカルAPICレジスタへの書き込みを行う
static void
lapicw(int index, int value)
{
  lapic[index] = value;
  lapic[ID];  // 読み込むことで書き込み完了を待つ
}

// APICのセットアップ
void
lapicinit(void)
{
  // ローカルAPICのアドレスが見つかっていない場合
  if(!lapic)
    return;

  // ローカルAPICを有効化; 仮の割り込みベクタをセットする.
  lapicw(SVR, ENABLE | (T_IRQ0 + IRQ_SPURIOUS));

  // タイマはlapic[TICR]からバスの周波数でカウントダウンし、割り込みを発生させる。
  // もしxv6がより正確な時間管理を行う場合には外部タイマソースを用いて調整する。
  lapicw(TDCR, X1); // カウンタを1で割る
  lapicw(TIMER, PERIODIC | (T_IRQ0 + IRQ_TIMER));
  lapicw(TICR, 10000000); // 初期値を設定

  // 論理割り込みラインを無効に
  lapicw(LINT0, MASKED);
  lapicw(LINT1, MASKED);

  // 割り込みエントリを提供するマシン上での
  // パフォーマンスカウンタのオーバーフロー割り込みを無効化
  if(((lapic[VER]>>16) & 0xFF) >= 4)
    lapicw(PCINT, MASKED);

  // マップエラー割り込みをIRQ_ERRORへ
  lapicw(ERROR, T_IRQ0 + IRQ_ERROR);

  // エラーステータスレジスタをクリア(requires back-to-back writes).
  lapicw(ESR, 0);
  lapicw(ESR, 0);

  // 割り込みに応答する
  lapicw(EOI, 0);

  // 同期的な任意のIDに対して初期レベルの無効化を行う
  lapicw(ICRHI, 0);
  lapicw(ICRLO, BCAST | INIT | LEVEL);
  while(lapic[ICRLO] & DELIVS)
    ;

  // APICに対する割り込みを有効化する(CPUに対してではなく)
  lapicw(TPR, 0);
}

// ローカルAPICのIDを取得する
int
lapicid(void)
{
  if (!lapic)
    return 0;
  return lapic[ID] >> 24; // 上位8bitがIDになっている
}

// Acknowledge interrupt.
void
lapiceoi(void)
{
  if(lapic)
    lapicw(EOI, 0);
}

// 指定音マイクロ秒スピンする
// 物理機器では動的に調節される
void
microdelay(int us)
{
}

#define CMOS_PORT    0x70
#define CMOS_RETURN  0x71

// "addr"で指定したエントリコードからAP(apicid)を起動する
// 詳細: Appendix B of MultiProcessor Specification.
void
lapicstartap(uchar apicid, uint addr)
{
  int i;
  ushort *wrv;

  // ブートストラッププロセッサはCMOSをシャットダウンコード(0A)及び
  // APのスタートアップコード指しているリセットベクタ(DWORD based at 40:67)
  // を初期化する。

  // http://helppc.netcore2k.net/hardware/cmos-clock
  outb(CMOS_PORT, 0xF);  // 0x0F: Shutdown status byte
  outb(CMOS_PORT+1, 0x0A); // JMP DWORD request without INT init

  wrv = (ushort*)P2V((0x40<<4 | 0x67));  // リセットベクタを指す
  // リセットベクタに対してアドレス値を設定する
  wrv[0] = 0;
  wrv[1] = addr >> 4;

  // "Universal startup algorithm."
  // 他のCPUをリセットするため初期化割り込みを送信する。
  lapicw(ICRHI, apicid<<24);
  lapicw(ICRLO, INIT | LEVEL | ASSERT);
  microdelay(200);
  lapicw(ICRLO, INIT | LEVEL);
  microdelay(100);    // 10ミリ秒である必要がある,Bochsでは遅すぎる

  // コードを実行するため起動のIPIを2回送信する。
  // 一般的なハードウェアは停止状態の時にのみ起動コードを受け付ける。
  // それゆえ２回目の起動コードは無視される、しかしこれが
  // Intelの公式アルゴリズムの一部で使用されている。

  // 2回目を送信
  for(i = 0; i < 2; i++){
    lapicw(ICRHI, apicid<<24);
    lapicw(ICRLO, STARTUP | (addr>>12));
    microdelay(200);
  }
}

#define CMOS_STATA   0x0a
#define CMOS_STATB   0x0b
#define CMOS_UIP    (1 << 7)        // RTC update in progress

#define SECS    0x00
#define MINS    0x02
#define HOURS   0x04
#define DAY     0x07
#define MONTH   0x08
#define YEAR    0x09

static uint
cmos_read(uint reg)
{
  outb(CMOS_PORT,  reg);
  microdelay(200);

  return inb(CMOS_RETURN);
}

static void
fill_rtcdate(struct rtcdate *r)
{
  r->second = cmos_read(SECS);
  r->minute = cmos_read(MINS);
  r->hour   = cmos_read(HOURS);
  r->day    = cmos_read(DAY);
  r->month  = cmos_read(MONTH);
  r->year   = cmos_read(YEAR);
}

// qemu seems to use 24-hour GWT and the values are BCD encoded
void
cmostime(struct rtcdate *r)
{
  struct rtcdate t1, t2;
  int sb, bcd;

  sb = cmos_read(CMOS_STATB);

  bcd = (sb & (1 << 2)) == 0;

  // make sure CMOS doesn't modify time while we read it
  for(;;) {
    fill_rtcdate(&t1);
    if(cmos_read(CMOS_STATA) & CMOS_UIP)
        continue;
    fill_rtcdate(&t2);
    if(memcmp(&t1, &t2, sizeof(t1)) == 0)
      break;
  }

  // convert
  if(bcd) {
#define    CONV(x)     (t1.x = ((t1.x >> 4) * 10) + (t1.x & 0xf))
    CONV(second);
    CONV(minute);
    CONV(hour  );
    CONV(day   );
    CONV(month );
    CONV(year  );
#undef     CONV
  }

  *r = t1;
  r->year += 2000;
}
