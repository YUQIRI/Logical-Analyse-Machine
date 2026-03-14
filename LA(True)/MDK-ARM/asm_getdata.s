BUFFER_SIZE equ 1024 
 
 
                THUMB 
                AREA    |.text|, CODE, READONLY 
 
; sample_function handler 
sample_function    PROC 
                 EXPORT  sample_function 
                IMPORT g_rxdata_buf 
                IMPORT g_rxcnt_buf 
                IMPORT g_cur_pos 
                IMPORT g_cur_sample_cnt 
                IMPORT get_stop_cmd 
                IMPORT g_convreted_sample_count 
                  
    PUSH     {R4, R5, R6, R7, R8, R9, R10, R11, R12, LR} 
    LDR R0, =g_rxdata_buf  ; 得到这些变量的地址,并不是得到它们的值 
    LDR R1, =g_rxcnt_buf   ; 得到g_rxcnt_buf变量的地址,并不是得到它的值 
    LDR R2, =g_cur_pos     ; 得到g_cur_pos变量的地址,并不是得到它的值 
    LDR R2, [R2]           ; 得到g_cur_pos变量的值 
    LDR R3, =g_cur_sample_cnt 
    LDR R3, [R3] 
    LDR R4, =get_stop_cmd 
    LDR R5, =g_convreted_sample_count 
    LDR R5, [R5] 
 
    LDR R8, [R0]  ; pre_data 
    LDR R10, =BUFFER_SIZE 
 
    LDR  R6, =0x40010C08  
     
    ; 设置PA15的值备用 
    LDR R11, =0X40010810 
    LDR R12, =(1<<15) 
    LDR LR, =(1<<31) 
Loop     
    ; 设置PA15输出高电平 
    STR R12, [R11] 
 
    LDRH R7, [R6]  ; 读GPIOB_IDR 
    LSR R7, #8    ; data = (*data_reg) >> 8; 
    CMP R7, R8 
    ADDNE R2, #1  ; g_cur_pos += (data != pre_data)? 1 : 0; 
    STRB R7, [R0, R2] ; g_rxdata_buf[g_cur_pos] = data;     
    MOV R8, R7        ; pre_data = data 
    LDR R7, [R1, R2, LSL #2] ; R7 = g_rxcnt_buf[g_cur_pos] 
    ADD R7, #1 
    STR R7, [R1, R2, LSL #2] ; g_rxcnt_buf[g_cur_pos]++; 
    ADD R3, #1    ; g_cur_sample_cnt++; 
 
    CMP R3, R5    ; if (g_cur_sample_cnt >= g_convreted_sample_count) break; 
    BGE LoopDone 
 
    LDR R7, [R4]  ; R7 = get_stop_cmd 
    CMP R7, #0    ; if (get_stop_cmd) break; 
    BNE LoopDone 
 
    CMP R2, R10    ; if (g_cur_pos >= BUFFER_SIZE) break; 
    BGE LoopDone 
 
    NOP 
    NOP         ; 延时, 凑出2MHz 
     
    ; 设置PA15输出高电平 
    STR LR, [R11] 
         
    B Loop 
     
LoopDone 
    LDR R0, =g_cur_pos     ; 得到g_cur_pos变量的地址,并不是得到它的值 
    STR R2, [R0]           ; 保存g_cur_pos变量的值 
    LDR R0, =g_cur_sample_cnt 
    STR R3, [R0]           ; 保存g_cur_sample_cnt变量的值 
     
	POP     {R4, R5, R6, R7, R8, R9, R10, R11, R12, PC} 
	ENDP 