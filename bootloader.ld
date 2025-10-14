ENTRY(UefiEntry)
SECTIONS{
	.text : {*(.text*)}
	.data : {*(.data*)}
	.bss : {*(.bss*)}
	
}
