// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void consputc(int);

// ??
static int panicked = 0;

// コンソール用のロック
static struct {
  struct spinlock lock;
  int locking; // ロックが必要かどうか
} cons;

static void
printint(int xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}
//PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
  int i, c, locking;
  uint *argp;
  char *s;

  locking = cons.locking;
  if(locking)
    acquire(&cons.lock);

  if (fmt == 0)
    panic("null fmt");

  argp = (uint*)(void*)(&fmt + 1);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(*argp++, 10, 1);
      break;
    case 'x':
    case 'p':
      printint(*argp++, 16, 0);
      break;
    case 's':
      if((s = (char*)*argp++) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if(locking)
    release(&cons.lock);
}

void
panic(char *s)
{
  int i;
  uint pcs[10];

  cli(); // 割り込みの禁止
  cons.locking = 0;
  // use lapiccpunum so that we can call panic from mycpu()
  cprintf("lapicid %d: panic: ", lapicid());
  cprintf(s);
  cprintf("\n");
  getcallerpcs(&s, pcs);
  for(i=0; i<10; i++)
    cprintf(" %p", pcs[i]);
  panicked = 1; // freeze other CPU
  for(;;)
    ;
}

//PAGEBREAK: 50
#define BACKSPACE 0x100 // charcode of backspace

// https://wiki.osdev.org/VGA_Hardware#Port_0x3C4.2C_0x3CE.2C_0x3D4
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static void
cgaputc(int c)
{
  int pos;

  // Cursor position: col + 80*row.
  outb(CRTPORT, 14);
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);

  if(c == '\n')
    pos += 80 - pos%80;
  else if(c == BACKSPACE){
    if(pos > 0) --pos;
  } else
    crt[pos++] = (c&0xff) | 0x0700;  // black on white

  if(pos < 0 || pos > 25*80)
    panic("pos under/overflow");

  if((pos/80) >= 24){  // Scroll up.
    memmove(crt, crt+80, sizeof(crt[0])*23*80);
    pos -= 80;
    memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
  }

  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
  crt[pos] = ' ' | 0x0700;
}

void
consputc(int c)
{
  if(panicked){
    cli(); // 割り込みを禁止に無限ループ
    for(;;)
      ;
  }

  // バックスペースの場合、単語区切り文字で挟んだスペースを送信
  if(c == BACKSPACE){
    uartputc('\b'); uartputc(' '); uartputc('\b');
  } else
    uartputc(c); // 指定の文字をCOMポートに送信
  cgaputc(c);
}

// 入力用のバッファ
#define INPUT_BUF 128
struct {
  char buf[INPUT_BUF]; // リングバッファ
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} input;

// "Ctrl"とxを押した場合のキーコード
#define C(x)  ((x)-'@')  // Control-x

void
consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0;

  acquire(&cons.lock);
  while((c = getc()) >= 0){
    switch(c){
    case C('P'):  // Process listing.
      // procdump() locks cons.lock indirectly; invoke later
      doprocdump = 1;
      break;
    case C('U'):  // Kill line.
      while(input.e != input.w &&
            input.buf[(input.e-1) % INPUT_BUF] != '\n'){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    case C('H'): case '\x7f':  // Backspace
      if(input.e != input.w){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    default:
      if(c != 0 && input.e-input.r < INPUT_BUF){
        c = (c == '\r') ? '\n' : c;
        input.buf[input.e++ % INPUT_BUF] = c;
        consputc(c);
        if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
          input.w = input.e;
          wakeup(&input.r);
        }
      }
      break;
    }
  }
  release(&cons.lock);
  if(doprocdump) {
    procdump();  // now call procdump() wo. cons.lock held
  }
}

// 
int
consoleread(struct inode *ip, char *dst, int n)
{
  uint target;
  int c;

  iunlock(ip); // inodeのロックを開放
  target = n; // 指定された書き込み文字数
  acquire(&cons.lock); // コンソール用のロックを取得

  while(n > 0){
    // 読み込みと書き込みのインデックスが同じ場合
    // それ以上何も入力されていないことが確実なのでスリープする
    while(input.r == input.w){
      // カレントプロセスが既にKILLされている場合
      if(myproc()->killed){
        release(&cons.lock); // コンソール用のロックを開放
        ilock(ip); // inodeを再度ロック
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    
    // 読み込み用のインデックスを用いてリングバッファからデータを読み込む
    c = input.buf[input.r++ % INPUT_BUF];

    if(c == C('D')){  // EOFの場合
      if(n < target){
        // インデックスを戻すことで次回のために"Ctrl+D"を保存しておく
        // これにより呼び出し側が0バイトの結果を受け取ることが確実となる
        input.r--;
      }
      break;
    }
    *dst++ = c; // バッファから読んだものを書き込む
    --n; // 指定書き込み回数をデクリメント

    // 改行の場合は読み込み完了
    if(c == '\n')
      break;
  }
  release(&cons.lock); // コンソール用のロックを開放
  ilock(ip); // inodeをロック

  return target - n;
}

// コンソール用のwrite関数
int
consolewrite(struct inode *ip, char *buf, int n)
{
  int i;

  iunlock(ip); // 指定inodeのスリープロックを開放し、その上でスリープしているプロセスを起床させる
  acquire(&cons.lock); // スピンロックを取得
  for(i = 0; i < n; i++)
    consputc(buf[i] & 0xff);
  release(&cons.lock); // スピンロックを開放
  ilock(ip); // 指定のinodeをロックする(読み込まれていない場合はディスクから読み込む)

  return n;
}

// コンソールの初期化及びキーボード割り込みのルーティング処理
void
consoleinit(void)
{
  // コンソール用のロックを初期化
  initlock(&cons.lock, "console");

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;

  // キーボードからの割り込みをCPUの0番にルーティングする
  ioapicenable(IRQ_KBD, 0);
}

