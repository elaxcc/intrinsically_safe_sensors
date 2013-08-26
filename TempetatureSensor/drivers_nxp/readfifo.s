        AREA MCIFIFO, CODE, READONLY

		EXPORT MCI_ReadFifo
		EXPORT MCI_WriteFifo

		; int MCI_ReadFifo(unsigned int * dest);
		; int MCI_WriteFifo(unsigned int * src);
src     RN 4
dst     RN 5

MCI_ReadFifo
		STMFD   sp!,{src,dst,r12,lr}
		LDR     src, mci_fifo
		MOV     dst, r0
		LDMIA   src, {r0-r3}
		STMIA   dst!, {r0-r3}
		LDMIA   src, {r0-r3}
		STMIA   dst!, {r0-r3}
		MOV     r0, dst
		LDMFD   sp!,{src,dst,r12,pc}

MCI_WriteFifo
		STMFD   sp!,{src,dst, r12,lr}
		LDR     dst, mci_fifo
		MOV     src, r0
		LDMIA   src!, {r0-r3}
		STMIA   dst, {r0-r3}
		LDMIA   src!, {r0-r3}
		STMIA   dst, {r0-r3}
		MOV     r0, src
		LDMFD   sp!,{src,dst,r12 ,pc}

mci_fifo
        DCD     0xE008C080
        END     