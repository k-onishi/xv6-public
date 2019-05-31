/* Simple linker script for the JOS kernel.
   See the GNU ld 'info' manual ("info ld") to learn the syntax. */

/* フォーマットの指定、左からデフォルト、ビッグエンディアン、リトルエンディアンの場合のフォーマットを記述する*/
OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")

/* アーキテクチャ */
OUTPUT_ARCH(i386)

/* エントリポイントの指定 */
ENTRY(_start)

SECTIONS
{
	/* 
	 * カーネルをこのアドレスにリンクさせる
	 * "."はカレントアドレスを指す(ロケーションカウンタと呼ばれる)
	 */
  /* KERNLINKと同値でなければならない */
	. = 0x80100000; /* カレントアドレスを更新 */

	/* .testセクションには含むセクションを指定 */
	.text : AT(0x100000) { /* LMA(Load Memory Address)を指定 */
		*(.text .stub .text.* .gnu.linkonce.t.*)
	}

	/* etext: textセクションの末尾 */
	/* PROVIDE(): 変数が未定義の場合、値を設定する */
	PROVIDE(etext = .);	/* Define the 'etext' symbol to this value */

	/* 読み込み専用データセクションに含むセクションを指定 */
	.rodata : {
		*(.rodata .rodata.* .gnu.linkonce.r.*)
	}

	/* カーネルメモリ内にデバッグ情報を含める */
	.stab : {
		PROVIDE(__STAB_BEGIN__ = .); /* シンボル */
		*(.stab);
		PROVIDE(__STAB_END__ = .); /* シンボル */
		BYTE(0) /* リンカに対して当該セクション用のスペースを配置することを矯正する */
	}

	.stabstr : {
		PROVIDE(__STABSTR_BEGIN__ = .); /* シンボル */
		*(.stabstr);
		PROVIDE(__STABSTR_END__ = .); /* シンボル */
		BYTE(0) /* リンカに対して当該セクション用のスペースを配置することを矯正する */
	}

	/* データセグメントが次のページに配置されるようアドレスを調整する */
	. = ALIGN(0x1000);

	/*
	 * 一般的にUnixのリンカでは仮想シンボルを提供し、etext, edata, endがそれぞれ、
	 * test, data, bssセグメントの終端を表す。カーネルのマッピングではデータセクションの
	 * 開始アドレスが必要だが、マッピングがtext及びdataセクションの間にあるrodataが配置される前に
	 * 始まるため、これは一般的なシンボルではない。
	 */
	PROVIDE(data = .);

	/* データセグメント */
	.data : {
		*(.data)
	}

	PROVIDE(edata = .); /* データセグメントの終端 */

	/* bssセグメント */
	.bss : {
		*(.bss)
	}

	PROVIDE(end = .); /* bssセグメントの終端 */

	/* このオブジェクトはリンクしない */
	/DISCARD/ : {
		*(.eh_frame .note.GNU-stack)
	}
}
