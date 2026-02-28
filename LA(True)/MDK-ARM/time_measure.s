
                THUMB

                AREA    |.text|, CODE, READONLY

; Time_Measure handler
Time_Measure    PROC
					EXPORT Time_Measure
					
				;set PA12
				LDR R1, =0x40010810
				LDR R0, =(1<<12)
				STR R0,[R1]
				
				;set PA8 
				LDR R1, = 0x40010C08
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				LDR R0, [R1] ; read GPIOB_IDR
				
				;reset PA12
				LDR R1, =0x40010810
				LDR R0, =(1<<28)
				STR R0,[R1]
				
				;back
				BX LR
		
                 ENDP
