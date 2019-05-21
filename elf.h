// Format of an ELF executable file

// 以下のサイトではELFヘッダやプログラムヘッダの詳細が丁寧に解説されている
// http://caspar.hazymoon.jp/OpenBSD/annex/elf.html

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

// ELFヘッダ
struct elfhdr {
  uint magic;  // 固定値("\x7FELF")。マジックナンバーを呼ばれる
  /**
   * [0] : バイナリの対応アーキテクチャ
   * [1] : プロセッサのバイトオーダ
   * [2] : ELFのVersion番号
   * [3] : OS及びABIの認識に用いる
   * [4] : APIのVersion
   * [5] : パディングの開始位置
   * [6] : パディング
   * [7] : パディング
   * [8] : パディング
   * [9] : パディング
   * [10]: パディング
   * [11]: パディング
   */
  uchar elf[12];
  ushort type; // ファイルタイプ(未知|再配置可能|実行可能|共有オブジェクト|コアファイル)
  ushort machine; // CPUアーキテクチャ
  uint version; // ファイルのバージョン
  uint entry; // プロセスの開始時にeipに読み込む仮想アドレス
  uint phoff; // プログラムヘッダテーブル位置のファイルオフセット値(存在しない場合は"0")(byte)
  uint shoff; // セクションヘッダテーブル位置のファイルオフセット値(存在しない場合は"0")(byte)
  uint flags; // プロセッサ固有フラグ
  ushort ehsize; // ELFヘッダのサイズ(byte)
  ushort phentsize; // プログラムヘッダテーブルのエントリサイズ
  ushort phnum; // プログラムヘッダテーブルが保持するエントリ数
  ushort shentsize; // セクションヘッダテーブルのエントリのサイズ
  ushort shnum; // セクションヘッダテーブルのエントリ数
  ushort shstrndx; // セクションヘッダテーブルのセクション名文字列テーブルに対応するエントリへのインデックス
};

// プログラムヘッダ
struct proghdr {
  uint type; // セグメントの種類
  uint off; // セグメントのファイルのオフセット(ELF_PROG_LOAD)
  uint vaddr; // セグメントの仮想アドレス
  uint paddr; // セグメントの物理アドレス
  uint filesz; // セグメントのファイルイメージのサイズ(Byte)
  uint memsz; // セグメントのメモリイメージのサイズ(Byte)
  uint flags; // セグメントの属性(ELF_PROG_FLAG_EXEC | ELF_PROG_FLAG_WRITE | ELF_PROG_FLAG_READ)
  uint align; // アラインメント単位
};

// Values for Proghdr type
#define ELF_PROG_LOAD           // ロード可能

// Flag bits for Proghdr flags
#define ELF_PROG_FLAG_EXEC      // 実行可能
#define ELF_PROG_FLAG_WRITE     // 書き込み可能
#define ELF_PROG_FLAG_READ      // 読み込み可能
