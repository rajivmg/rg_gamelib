#if 0
;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Position              0   xyzw        0     NONE   float   xyzw
; NORMAL                   0   xyz         1     NONE   float   xyz 
; COLOR                    0   xyzw        2     NONE   float   xyzw
;
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; TEXCOORD                 0   xyzw        0     NONE   float   xyzw
; TEXCOORD                 1   xyz         1     NONE   float   xyz 
; COLOR                    0   xyzw        2     NONE   float   xyzw
; SV_Position              0   xyzw        3      POS   float   xyzw
;
; shader debug name: Compiled\BasicEffect_VSBasicPixelLightingVc.pdb
; shader hash: 60a34f643ff673dd9fcfcdf8c197cf62
;
; Pipeline Runtime Information: 
;
; Vertex Shader
; OutputPositionPresent=1
;
;
; Input signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; SV_Position              0                              
; NORMAL                   0                              
; COLOR                    0                              
;
; Output signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; TEXCOORD                 0                 linear       
; TEXCOORD                 1                 linear       
; COLOR                    0                 linear       
; SV_Position              0          noperspective       
;
; Buffer Definitions:
;
; cbuffer 
; {
;
;   [416 x i8] (type annotation not present)
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
;                                   cbuffer      NA          NA     CB0            cb0     1
;
;
; ViewId state:
;
; Number of inputs: 12, outputs: 16
; Outputs dependent on ViewId: {  }
; Inputs contributing to computation of Outputs:
;   output 0 depends on inputs: { 0, 1, 2, 3 }
;   output 1 depends on inputs: { 0, 1, 2, 3 }
;   output 2 depends on inputs: { 0, 1, 2, 3 }
;   output 3 depends on inputs: { 0, 1, 2, 3 }
;   output 4 depends on inputs: { 4, 5, 6 }
;   output 5 depends on inputs: { 4, 5, 6 }
;   output 6 depends on inputs: { 4, 5, 6 }
;   output 8 depends on inputs: { 8 }
;   output 9 depends on inputs: { 9 }
;   output 10 depends on inputs: { 10 }
;   output 11 depends on inputs: { 11 }
;   output 12 depends on inputs: { 0, 1, 2, 3 }
;   output 13 depends on inputs: { 0, 1, 2, 3 }
;   output 14 depends on inputs: { 0, 1, 2, 3 }
;   output 15 depends on inputs: { 0, 1, 2, 3 }
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%hostlayout.Parameters = type { <4 x float>, <3 x float>, <3 x float>, float, [3 x <3 x float>], [3 x <3 x float>], [3 x <3 x float>], <3 x float>, <3 x float>, <4 x float>, [4 x <4 x float>], [3 x <3 x float>], [4 x <4 x float>] }

define void @VSBasicPixelLightingVc() {
  %1 = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 2, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %2 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %3 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %4 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %5 = call float @dx.op.loadInput.f32(i32 4, i32 2, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %6 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %7 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %8 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %9 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %10 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %11 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %12 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %13 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 22)  ; CBufferLoadLegacy(handle,regIndex)
  %14 = extractvalue %dx.types.CBufRet.f32 %13, 0
  %15 = extractvalue %dx.types.CBufRet.f32 %13, 1
  %16 = extractvalue %dx.types.CBufRet.f32 %13, 2
  %17 = extractvalue %dx.types.CBufRet.f32 %13, 3
  %18 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 23)  ; CBufferLoadLegacy(handle,regIndex)
  %19 = extractvalue %dx.types.CBufRet.f32 %18, 0
  %20 = extractvalue %dx.types.CBufRet.f32 %18, 1
  %21 = extractvalue %dx.types.CBufRet.f32 %18, 2
  %22 = extractvalue %dx.types.CBufRet.f32 %18, 3
  %23 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 24)  ; CBufferLoadLegacy(handle,regIndex)
  %24 = extractvalue %dx.types.CBufRet.f32 %23, 0
  %25 = extractvalue %dx.types.CBufRet.f32 %23, 1
  %26 = extractvalue %dx.types.CBufRet.f32 %23, 2
  %27 = extractvalue %dx.types.CBufRet.f32 %23, 3
  %28 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 25)  ; CBufferLoadLegacy(handle,regIndex)
  %29 = extractvalue %dx.types.CBufRet.f32 %28, 0
  %30 = extractvalue %dx.types.CBufRet.f32 %28, 1
  %31 = extractvalue %dx.types.CBufRet.f32 %28, 2
  %32 = extractvalue %dx.types.CBufRet.f32 %28, 3
  %33 = fmul fast float %14, %9
  %34 = call float @dx.op.tertiary.f32(i32 46, float %10, float %15, float %33)  ; FMad(a,b,c)
  %35 = call float @dx.op.tertiary.f32(i32 46, float %11, float %16, float %34)  ; FMad(a,b,c)
  %36 = call float @dx.op.tertiary.f32(i32 46, float %12, float %17, float %35)  ; FMad(a,b,c)
  %37 = fmul fast float %19, %9
  %38 = call float @dx.op.tertiary.f32(i32 46, float %10, float %20, float %37)  ; FMad(a,b,c)
  %39 = call float @dx.op.tertiary.f32(i32 46, float %11, float %21, float %38)  ; FMad(a,b,c)
  %40 = call float @dx.op.tertiary.f32(i32 46, float %12, float %22, float %39)  ; FMad(a,b,c)
  %41 = fmul fast float %24, %9
  %42 = call float @dx.op.tertiary.f32(i32 46, float %10, float %25, float %41)  ; FMad(a,b,c)
  %43 = call float @dx.op.tertiary.f32(i32 46, float %11, float %26, float %42)  ; FMad(a,b,c)
  %44 = call float @dx.op.tertiary.f32(i32 46, float %12, float %27, float %43)  ; FMad(a,b,c)
  %45 = fmul fast float %29, %9
  %46 = call float @dx.op.tertiary.f32(i32 46, float %10, float %30, float %45)  ; FMad(a,b,c)
  %47 = call float @dx.op.tertiary.f32(i32 46, float %11, float %31, float %46)  ; FMad(a,b,c)
  %48 = call float @dx.op.tertiary.f32(i32 46, float %12, float %32, float %47)  ; FMad(a,b,c)
  %49 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %50 = extractvalue %dx.types.CBufRet.f32 %49, 0
  %51 = extractvalue %dx.types.CBufRet.f32 %49, 1
  %52 = extractvalue %dx.types.CBufRet.f32 %49, 2
  %53 = extractvalue %dx.types.CBufRet.f32 %49, 3
  %54 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %55 = extractvalue %dx.types.CBufRet.f32 %54, 0
  %56 = extractvalue %dx.types.CBufRet.f32 %54, 1
  %57 = extractvalue %dx.types.CBufRet.f32 %54, 2
  %58 = extractvalue %dx.types.CBufRet.f32 %54, 3
  %59 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %60 = extractvalue %dx.types.CBufRet.f32 %59, 0
  %61 = extractvalue %dx.types.CBufRet.f32 %59, 1
  %62 = extractvalue %dx.types.CBufRet.f32 %59, 2
  %63 = extractvalue %dx.types.CBufRet.f32 %59, 3
  %64 = fmul fast float %50, %9
  %65 = call float @dx.op.tertiary.f32(i32 46, float %10, float %51, float %64)  ; FMad(a,b,c)
  %66 = call float @dx.op.tertiary.f32(i32 46, float %11, float %52, float %65)  ; FMad(a,b,c)
  %67 = call float @dx.op.tertiary.f32(i32 46, float %12, float %53, float %66)  ; FMad(a,b,c)
  %68 = fmul fast float %55, %9
  %69 = call float @dx.op.tertiary.f32(i32 46, float %10, float %56, float %68)  ; FMad(a,b,c)
  %70 = call float @dx.op.tertiary.f32(i32 46, float %11, float %57, float %69)  ; FMad(a,b,c)
  %71 = call float @dx.op.tertiary.f32(i32 46, float %12, float %58, float %70)  ; FMad(a,b,c)
  %72 = fmul fast float %60, %9
  %73 = call float @dx.op.tertiary.f32(i32 46, float %10, float %61, float %72)  ; FMad(a,b,c)
  %74 = call float @dx.op.tertiary.f32(i32 46, float %11, float %62, float %73)  ; FMad(a,b,c)
  %75 = call float @dx.op.tertiary.f32(i32 46, float %12, float %63, float %74)  ; FMad(a,b,c)
  %76 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 19)  ; CBufferLoadLegacy(handle,regIndex)
  %77 = extractvalue %dx.types.CBufRet.f32 %76, 0
  %78 = extractvalue %dx.types.CBufRet.f32 %76, 1
  %79 = extractvalue %dx.types.CBufRet.f32 %76, 2
  %80 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 20)  ; CBufferLoadLegacy(handle,regIndex)
  %81 = extractvalue %dx.types.CBufRet.f32 %80, 0
  %82 = extractvalue %dx.types.CBufRet.f32 %80, 1
  %83 = extractvalue %dx.types.CBufRet.f32 %80, 2
  %84 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 21)  ; CBufferLoadLegacy(handle,regIndex)
  %85 = extractvalue %dx.types.CBufRet.f32 %84, 0
  %86 = extractvalue %dx.types.CBufRet.f32 %84, 1
  %87 = extractvalue %dx.types.CBufRet.f32 %84, 2
  %88 = fmul fast float %77, %6
  %89 = call float @dx.op.tertiary.f32(i32 46, float %7, float %78, float %88)  ; FMad(a,b,c)
  %90 = call float @dx.op.tertiary.f32(i32 46, float %8, float %79, float %89)  ; FMad(a,b,c)
  %91 = fmul fast float %81, %6
  %92 = call float @dx.op.tertiary.f32(i32 46, float %7, float %82, float %91)  ; FMad(a,b,c)
  %93 = call float @dx.op.tertiary.f32(i32 46, float %8, float %83, float %92)  ; FMad(a,b,c)
  %94 = fmul fast float %85, %6
  %95 = call float @dx.op.tertiary.f32(i32 46, float %7, float %86, float %94)  ; FMad(a,b,c)
  %96 = call float @dx.op.tertiary.f32(i32 46, float %8, float %87, float %95)  ; FMad(a,b,c)
  %97 = call float @dx.op.dot3.f32(i32 55, float %90, float %93, float %96, float %90, float %93, float %96)  ; Dot3(ax,ay,az,bx,by,bz)
  %98 = call float @dx.op.unary.f32(i32 25, float %97)  ; Rsqrt(value)
  %99 = fmul fast float %98, %90
  %100 = fmul fast float %98, %93
  %101 = fmul fast float %98, %96
  %102 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %103 = extractvalue %dx.types.CBufRet.f32 %102, 0
  %104 = extractvalue %dx.types.CBufRet.f32 %102, 1
  %105 = extractvalue %dx.types.CBufRet.f32 %102, 2
  %106 = extractvalue %dx.types.CBufRet.f32 %102, 3
  %107 = call float @dx.op.dot4.f32(i32 56, float %9, float %10, float %11, float %12, float %103, float %104, float %105, float %106)  ; Dot4(ax,ay,az,aw,bx,by,bz,bw)
  %108 = call float @dx.op.unary.f32(i32 7, float %107)  ; Saturate(value)
  %109 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %1, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %110 = extractvalue %dx.types.CBufRet.f32 %109, 3
  %111 = fmul fast float %110, %5
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %67)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %71)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %75)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %108)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float %99)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float %100)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 2, float %101)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 0, float %2)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 1, float %3)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 2, float %4)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 2, i32 0, i8 3, float %111)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 0, float %36)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 1, float %40)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 2, float %44)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 3, i32 0, i8 3, float %48)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #1

; Function Attrs: nounwind readnone
declare float @dx.op.dot3.f32(i32, float, float, float, float, float, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.dot4.f32(i32, float, float, float, float, float, float, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readnone
declare float @dx.op.tertiary.f32(i32, float, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #2

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind }
attributes #2 = { nounwind readonly }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.resources = !{!4}
!dx.viewIdState = !{!7}
!dx.entryPoints = !{!8}

!0 = !{!"dxcoob 1.7.2308.7 (69e54e290)"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 7}
!3 = !{!"vs", i32 6, i32 0}
!4 = !{null, null, !5, null}
!5 = !{!6}
!6 = !{i32 0, %hostlayout.Parameters* undef, !"", i32 0, i32 0, i32 1, i32 416, null}
!7 = !{[14 x i32] [i32 12, i32 16, i32 61455, i32 61455, i32 61455, i32 61455, i32 112, i32 112, i32 112, i32 0, i32 256, i32 512, i32 1024, i32 2048]}
!8 = !{void ()* @VSBasicPixelLightingVc, !"VSBasicPixelLightingVc", !9, !4, null}
!9 = !{!10, !17, null}
!10 = !{!11, !14, !16}
!11 = !{i32 0, !"SV_Position", i8 9, i8 0, !12, i8 0, i32 1, i8 4, i32 0, i8 0, !13}
!12 = !{i32 0}
!13 = !{i32 3, i32 15}
!14 = !{i32 1, !"NORMAL", i8 9, i8 0, !12, i8 0, i32 1, i8 3, i32 1, i8 0, !15}
!15 = !{i32 3, i32 7}
!16 = !{i32 2, !"COLOR", i8 9, i8 0, !12, i8 0, i32 1, i8 4, i32 2, i8 0, !13}
!17 = !{!18, !19, !21, !22}
!18 = !{i32 0, !"TEXCOORD", i8 9, i8 0, !12, i8 2, i32 1, i8 4, i32 0, i8 0, !13}
!19 = !{i32 1, !"TEXCOORD", i8 9, i8 0, !20, i8 2, i32 1, i8 3, i32 1, i8 0, !15}
!20 = !{i32 1}
!21 = !{i32 2, !"COLOR", i8 9, i8 0, !12, i8 2, i32 1, i8 4, i32 2, i8 0, !13}
!22 = !{i32 3, !"SV_Position", i8 9, i8 3, !12, i8 4, i32 1, i8 4, i32 3, i8 0, !13}

#endif

const unsigned char BasicEffect_VSBasicPixelLightingVc[] = {
  0x44, 0x58, 0x42, 0x43, 0x45, 0x8b, 0x2e, 0xff, 0x07, 0xd8, 0x79, 0x7d,
  0xee, 0x5a, 0x7c, 0xa4, 0x21, 0xc5, 0x91, 0x7f, 0x01, 0x00, 0x00, 0x00,
  0x84, 0x0e, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
  0x50, 0x00, 0x00, 0x00, 0xdc, 0x00, 0x00, 0x00, 0x88, 0x01, 0x00, 0x00,
  0xcc, 0x02, 0x00, 0x00, 0x04, 0x03, 0x00, 0x00, 0x40, 0x03, 0x00, 0x00,
  0x5c, 0x03, 0x00, 0x00, 0x53, 0x46, 0x49, 0x30, 0x08, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x53, 0x47, 0x31,
  0x84, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x07, 0x07, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7b, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x53, 0x56, 0x5f, 0x50, 0x6f, 0x73, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x00,
  0x4e, 0x4f, 0x52, 0x4d, 0x41, 0x4c, 0x00, 0x43, 0x4f, 0x4c, 0x4f, 0x52,
  0x00, 0x00, 0x00, 0x00, 0x4f, 0x53, 0x47, 0x31, 0xa4, 0x00, 0x00, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x07, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x91, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x97, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x54, 0x45, 0x58, 0x43, 0x4f, 0x4f, 0x52, 0x44,
  0x00, 0x43, 0x4f, 0x4c, 0x4f, 0x52, 0x00, 0x53, 0x56, 0x5f, 0x50, 0x6f,
  0x73, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x00, 0x50, 0x53, 0x56, 0x30,
  0x3c, 0x01, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00,
  0x03, 0x04, 0x00, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x18, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x00, 0x53, 0x56, 0x5f,
  0x50, 0x6f, 0x73, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x4e, 0x4f, 0x52,
  0x4d, 0x41, 0x4c, 0x00, 0x43, 0x4f, 0x4c, 0x4f, 0x52, 0x00, 0x54, 0x45,
  0x58, 0x43, 0x4f, 0x4f, 0x52, 0x44, 0x00, 0x54, 0x45, 0x58, 0x43, 0x4f,
  0x4f, 0x52, 0x44, 0x00, 0x43, 0x4f, 0x4c, 0x4f, 0x52, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x44, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x43, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x44, 0x00,
  0x03, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x44, 0x00, 0x03, 0x02, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x01, 0x01, 0x43, 0x00, 0x03, 0x02, 0x00, 0x00,
  0x2c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x44, 0x00,
  0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x03, 0x44, 0x03, 0x03, 0x04, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00,
  0x0f, 0xf0, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00,
  0x70, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
  0x00, 0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x52, 0x54, 0x53, 0x30,
  0x30, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00,
  0x1d, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x49, 0x4c, 0x44, 0x4e, 0x34, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x2f, 0x00, 0x43, 0x6f, 0x6d, 0x70, 0x69, 0x6c, 0x65, 0x64,
  0x5c, 0x42, 0x61, 0x73, 0x69, 0x63, 0x45, 0x66, 0x66, 0x65, 0x63, 0x74,
  0x5f, 0x56, 0x53, 0x42, 0x61, 0x73, 0x69, 0x63, 0x50, 0x69, 0x78, 0x65,
  0x6c, 0x4c, 0x69, 0x67, 0x68, 0x74, 0x69, 0x6e, 0x67, 0x56, 0x63, 0x2e,
  0x70, 0x64, 0x62, 0x00, 0x48, 0x41, 0x53, 0x48, 0x14, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x60, 0xa3, 0x4f, 0x64, 0x3f, 0xf6, 0x73, 0xdd,
  0x9f, 0xcf, 0xcd, 0xf8, 0xc1, 0x97, 0xcf, 0x62, 0x44, 0x58, 0x49, 0x4c,
  0x20, 0x0b, 0x00, 0x00, 0x60, 0x00, 0x01, 0x00, 0xc8, 0x02, 0x00, 0x00,
  0x44, 0x58, 0x49, 0x4c, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
  0x08, 0x0b, 0x00, 0x00, 0x42, 0x43, 0xc0, 0xde, 0x21, 0x0c, 0x00, 0x00,
  0xbf, 0x02, 0x00, 0x00, 0x0b, 0x82, 0x20, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x13, 0x00, 0x00, 0x00, 0x07, 0x81, 0x23, 0x91, 0x41, 0xc8, 0x04, 0x49,
  0x06, 0x10, 0x32, 0x39, 0x92, 0x01, 0x84, 0x0c, 0x25, 0x05, 0x08, 0x19,
  0x1e, 0x04, 0x8b, 0x62, 0x80, 0x18, 0x45, 0x02, 0x42, 0x92, 0x0b, 0x42,
  0xc4, 0x10, 0x32, 0x14, 0x38, 0x08, 0x18, 0x4b, 0x0a, 0x32, 0x62, 0x88,
  0x48, 0x90, 0x14, 0x20, 0x43, 0x46, 0x88, 0xa5, 0x00, 0x19, 0x32, 0x42,
  0xe4, 0x48, 0x0e, 0x90, 0x11, 0x23, 0xc4, 0x50, 0x41, 0x51, 0x81, 0x8c,
  0xe1, 0x83, 0xe5, 0x8a, 0x04, 0x31, 0x46, 0x06, 0x51, 0x18, 0x00, 0x00,
  0x08, 0x00, 0x00, 0x00, 0x1b, 0x8c, 0xe0, 0xff, 0xff, 0xff, 0xff, 0x07,
  0x40, 0x02, 0xa8, 0x0d, 0x84, 0xf0, 0xff, 0xff, 0xff, 0xff, 0x03, 0x20,
  0x6d, 0x30, 0x86, 0xff, 0xff, 0xff, 0xff, 0x1f, 0x00, 0x09, 0xa8, 0x00,
  0x49, 0x18, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x13, 0x82, 0x60, 0x42,
  0x20, 0x4c, 0x08, 0x06, 0x00, 0x00, 0x00, 0x00, 0x89, 0x20, 0x00, 0x00,
  0x2f, 0x00, 0x00, 0x00, 0x32, 0x22, 0x88, 0x09, 0x20, 0x64, 0x85, 0x04,
  0x13, 0x23, 0xa4, 0x84, 0x04, 0x13, 0x23, 0xe3, 0x84, 0xa1, 0x90, 0x14,
  0x12, 0x4c, 0x8c, 0x8c, 0x0b, 0x84, 0xc4, 0x4c, 0x10, 0x88, 0xc1, 0x08,
  0x40, 0x09, 0x00, 0x0a, 0x66, 0x00, 0xe6, 0x08, 0xc0, 0x60, 0x8e, 0x00,
  0x29, 0xc6, 0x40, 0x10, 0x44, 0x41, 0x90, 0x51, 0x0c, 0x80, 0x20, 0x88,
  0x62, 0x20, 0xa4, 0x20, 0x03, 0x31, 0x0c, 0xc3, 0x30, 0x0c, 0xa4, 0x94,
  0x61, 0x20, 0x06, 0x62, 0x8a, 0x32, 0x10, 0xc3, 0x30, 0x0c, 0xc3, 0x30,
  0x0c, 0xe4, 0x1c, 0x35, 0x5c, 0xfe, 0x84, 0x3d, 0x84, 0xe4, 0x73, 0x1b,
  0x55, 0xac, 0xc4, 0xe4, 0x17, 0xb7, 0x8d, 0x88, 0x61, 0x18, 0x06, 0x2a,
  0xee, 0x19, 0x2e, 0x7f, 0xc2, 0x1e, 0x42, 0xf2, 0x43, 0xa0, 0x19, 0x16,
  0x02, 0x05, 0x51, 0x21, 0x20, 0x42, 0x22, 0x68, 0x2a, 0xc5, 0x40, 0x0c,
  0xc3, 0x40, 0xd5, 0x1c, 0x41, 0x50, 0x0c, 0x89, 0x28, 0x08, 0xe2, 0x22,
  0x6c, 0x20, 0x60, 0x18, 0x81, 0x18, 0x86, 0x11, 0x86, 0x21, 0x07, 0xb8,
  0x24, 0xd8, 0xae, 0x3a, 0x1c, 0x69, 0x5a, 0x00, 0xcc, 0xa1, 0x26, 0x3f,
  0x05, 0x22, 0x80, 0x21, 0x26, 0x22, 0x92, 0x50, 0xb3, 0x71, 0xdc, 0xd0,
  0x75, 0x1d, 0xc7, 0x6d, 0x5e, 0xe7, 0xd1, 0x97, 0x0e, 0x04, 0x00, 0x00,
  0x13, 0x14, 0x72, 0xc0, 0x87, 0x74, 0x60, 0x87, 0x36, 0x68, 0x87, 0x79,
  0x68, 0x03, 0x72, 0xc0, 0x87, 0x0d, 0xaf, 0x50, 0x0e, 0x6d, 0xd0, 0x0e,
  0x7a, 0x50, 0x0e, 0x6d, 0x00, 0x0f, 0x7a, 0x30, 0x07, 0x72, 0xa0, 0x07,
  0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x71, 0xa0, 0x07, 0x73, 0x20, 0x07,
  0x6d, 0x90, 0x0e, 0x78, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e,
  0x71, 0x60, 0x07, 0x7a, 0x30, 0x07, 0x72, 0xd0, 0x06, 0xe9, 0x30, 0x07,
  0x72, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x76, 0x40, 0x07,
  0x7a, 0x60, 0x07, 0x74, 0xd0, 0x06, 0xe6, 0x10, 0x07, 0x76, 0xa0, 0x07,
  0x73, 0x20, 0x07, 0x6d, 0x60, 0x0e, 0x73, 0x20, 0x07, 0x7a, 0x30, 0x07,
  0x72, 0xd0, 0x06, 0xe6, 0x60, 0x07, 0x74, 0xa0, 0x07, 0x76, 0x40, 0x07,
  0x6d, 0xe0, 0x0e, 0x78, 0xa0, 0x07, 0x71, 0x60, 0x07, 0x7a, 0x30, 0x07,
  0x72, 0xa0, 0x07, 0x76, 0x40, 0x07, 0x43, 0x9e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86, 0x3c, 0x06, 0x10, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x79, 0x10, 0x20,
  0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0xf2, 0x28,
  0x40, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0xe4,
  0x61, 0x80, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60,
  0xc8, 0xe3, 0x00, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xc0, 0x90, 0x67, 0x02, 0x02, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x80, 0x21, 0x4f, 0x05, 0x04, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x43, 0x1e, 0x0c, 0x08, 0x80, 0x01, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xb2, 0x40, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
  0x32, 0x1e, 0x98, 0x14, 0x19, 0x11, 0x4c, 0x90, 0x8c, 0x09, 0x26, 0x47,
  0xc6, 0x04, 0x43, 0x22, 0x4a, 0x60, 0x04, 0xa0, 0x1c, 0x8a, 0xa1, 0x80,
  0x0d, 0xca, 0xa0, 0x3c, 0x8a, 0x80, 0x8a, 0x92, 0x18, 0x01, 0x28, 0x84,
  0x32, 0x28, 0x02, 0x02, 0x67, 0x00, 0x28, 0x1c, 0xcb, 0x61, 0xa0, 0x17,
  0xe4, 0x83, 0x17, 0xe4, 0x83, 0x17, 0xe4, 0x83, 0x17, 0xe4, 0x03, 0x78,
  0x80, 0x07, 0x78, 0x00, 0x40, 0x04, 0x84, 0x40, 0x30, 0x00, 0x41, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x79, 0x18, 0x00, 0x00, 0x68, 0x00, 0x00, 0x00,
  0x1a, 0x03, 0x4c, 0x90, 0x46, 0x02, 0x13, 0xc4, 0x8e, 0x0c, 0x6f, 0xec,
  0xed, 0x4d, 0x0c, 0x24, 0xc6, 0xe5, 0xc6, 0x45, 0x66, 0x06, 0x06, 0xc7,
  0xe5, 0x06, 0x04, 0xc5, 0x26, 0xa7, 0xac, 0x86, 0xa6, 0x4c, 0x26, 0x07,
  0x26, 0x65, 0x43, 0x10, 0x4c, 0x10, 0x88, 0x64, 0x82, 0x40, 0x28, 0x1b,
  0x84, 0x81, 0x98, 0x20, 0x10, 0xcb, 0x06, 0x61, 0x30, 0x28, 0xd8, 0xcd,
  0x4d, 0x10, 0x08, 0x66, 0xc3, 0x80, 0x24, 0xc4, 0x04, 0x01, 0x0c, 0x2c,
  0x02, 0x13, 0x04, 0xa2, 0xd9, 0x80, 0x10, 0x0b, 0x43, 0x10, 0x43, 0x03,
  0x6c, 0x08, 0x9c, 0x0d, 0x04, 0x00, 0x3c, 0xc0, 0x04, 0x21, 0x0c, 0xae,
  0x0d, 0x41, 0x34, 0x41, 0x10, 0x00, 0x5a, 0x56, 0x53, 0x42, 0x61, 0x73,
  0x69, 0x63, 0x50, 0x69, 0x78, 0x65, 0x6c, 0x4c, 0x69, 0x67, 0x68, 0x74,
  0x69, 0x6e, 0x67, 0x56, 0x63, 0x5c, 0xa6, 0xac, 0xbe, 0xa0, 0xde, 0xe6,
  0xd2, 0xe8, 0xd2, 0xde, 0xdc, 0x26, 0x08, 0x45, 0x34, 0x41, 0x28, 0xa4,
  0x0d, 0x01, 0x31, 0x41, 0x28, 0xa6, 0x09, 0x02, 0xe1, 0x4c, 0x10, 0x88,
  0x67, 0x83, 0xa0, 0x6d, 0x1b, 0x16, 0xa2, 0xb2, 0x2e, 0xec, 0x1a, 0x32,
  0xe2, 0xe2, 0x68, 0x38, 0x3d, 0x49, 0x35, 0x05, 0x31, 0x4d, 0x10, 0x0a,
  0x6a, 0x83, 0xa0, 0x19, 0x1b, 0x96, 0xc1, 0xb3, 0x2e, 0xec, 0x1a, 0xbe,
  0xe1, 0x02, 0x83, 0x09, 0x02, 0x01, 0xb1, 0x18, 0x7a, 0x62, 0x7a, 0x92,
  0xda, 0xb0, 0x88, 0xc1, 0x18, 0x58, 0x17, 0x76, 0x0d, 0x99, 0x18, 0x5c,
  0xdc, 0x86, 0xa1, 0x0b, 0x03, 0x32, 0x20, 0x42, 0x55, 0x84, 0x35, 0xf4,
  0xf4, 0x24, 0x45, 0x34, 0x41, 0x28, 0xaa, 0x0d, 0x0b, 0x61, 0x06, 0xd6,
  0x85, 0x9d, 0xc1, 0x90, 0x11, 0x17, 0xb7, 0x21, 0x18, 0x36, 0x2c, 0x83,
  0x19, 0x58, 0x57, 0x1a, 0x9c, 0xc1, 0xf0, 0x0d, 0x17, 0x18, 0x6c, 0x58,
  0xc4, 0x60, 0x0c, 0xac, 0x0b, 0x3b, 0x83, 0x21, 0x13, 0x83, 0x8b, 0xdb,
  0xb0, 0x68, 0x95, 0xf5, 0x61, 0xd9, 0x90, 0x69, 0x17, 0xb7, 0x81, 0x40,
  0x03, 0x35, 0x58, 0x03, 0x36, 0xd8, 0x30, 0x94, 0x41, 0x1b, 0x00, 0x1b,
  0x8a, 0x89, 0x72, 0x03, 0x08, 0xa8, 0xc2, 0xc6, 0x66, 0xd7, 0xe6, 0x92,
  0x46, 0x56, 0xe6, 0x46, 0x37, 0x25, 0x08, 0xaa, 0x90, 0xe1, 0xb9, 0xd8,
  0x95, 0xc9, 0xcd, 0xa5, 0xbd, 0xb9, 0x4d, 0x09, 0x88, 0x26, 0x64, 0x78,
  0x2e, 0x76, 0x61, 0x6c, 0x76, 0x65, 0x72, 0x53, 0x02, 0xa3, 0x0e, 0x19,
  0x9e, 0xcb, 0x1c, 0x5a, 0x18, 0x59, 0x99, 0x5c, 0xd3, 0x1b, 0x59, 0x19,
  0xdb, 0x94, 0x20, 0x29, 0x43, 0x86, 0xe7, 0x22, 0x57, 0x36, 0xf7, 0x56,
  0x27, 0x37, 0x56, 0x36, 0x37, 0x25, 0x78, 0xea, 0x90, 0xe1, 0xb9, 0xd8,
  0xa5, 0x95, 0xdd, 0x25, 0x91, 0x4d, 0xd1, 0x85, 0xd1, 0x95, 0x4d, 0x09,
  0xa2, 0x3a, 0x64, 0x78, 0x2e, 0x65, 0x6e, 0x74, 0x72, 0x79, 0x50, 0x6f,
  0x69, 0x6e, 0x74, 0x73, 0x53, 0x02, 0x37, 0x00, 0x79, 0x18, 0x00, 0x00,
  0x4c, 0x00, 0x00, 0x00, 0x33, 0x08, 0x80, 0x1c, 0xc4, 0xe1, 0x1c, 0x66,
  0x14, 0x01, 0x3d, 0x88, 0x43, 0x38, 0x84, 0xc3, 0x8c, 0x42, 0x80, 0x07,
  0x79, 0x78, 0x07, 0x73, 0x98, 0x71, 0x0c, 0xe6, 0x00, 0x0f, 0xed, 0x10,
  0x0e, 0xf4, 0x80, 0x0e, 0x33, 0x0c, 0x42, 0x1e, 0xc2, 0xc1, 0x1d, 0xce,
  0xa1, 0x1c, 0x66, 0x30, 0x05, 0x3d, 0x88, 0x43, 0x38, 0x84, 0x83, 0x1b,
  0xcc, 0x03, 0x3d, 0xc8, 0x43, 0x3d, 0x8c, 0x03, 0x3d, 0xcc, 0x78, 0x8c,
  0x74, 0x70, 0x07, 0x7b, 0x08, 0x07, 0x79, 0x48, 0x87, 0x70, 0x70, 0x07,
  0x7a, 0x70, 0x03, 0x76, 0x78, 0x87, 0x70, 0x20, 0x87, 0x19, 0xcc, 0x11,
  0x0e, 0xec, 0x90, 0x0e, 0xe1, 0x30, 0x0f, 0x6e, 0x30, 0x0f, 0xe3, 0xf0,
  0x0e, 0xf0, 0x50, 0x0e, 0x33, 0x10, 0xc4, 0x1d, 0xde, 0x21, 0x1c, 0xd8,
  0x21, 0x1d, 0xc2, 0x61, 0x1e, 0x66, 0x30, 0x89, 0x3b, 0xbc, 0x83, 0x3b,
  0xd0, 0x43, 0x39, 0xb4, 0x03, 0x3c, 0xbc, 0x83, 0x3c, 0x84, 0x03, 0x3b,
  0xcc, 0xf0, 0x14, 0x76, 0x60, 0x07, 0x7b, 0x68, 0x07, 0x37, 0x68, 0x87,
  0x72, 0x68, 0x07, 0x37, 0x80, 0x87, 0x70, 0x90, 0x87, 0x70, 0x60, 0x07,
  0x76, 0x28, 0x07, 0x76, 0xf8, 0x05, 0x76, 0x78, 0x87, 0x77, 0x80, 0x87,
  0x5f, 0x08, 0x87, 0x71, 0x18, 0x87, 0x72, 0x98, 0x87, 0x79, 0x98, 0x81,
  0x2c, 0xee, 0xf0, 0x0e, 0xee, 0xe0, 0x0e, 0xf5, 0xc0, 0x0e, 0xec, 0x30,
  0x03, 0x62, 0xc8, 0xa1, 0x1c, 0xe4, 0xa1, 0x1c, 0xcc, 0xa1, 0x1c, 0xe4,
  0xa1, 0x1c, 0xdc, 0x61, 0x1c, 0xca, 0x21, 0x1c, 0xc4, 0x81, 0x1d, 0xca,
  0x61, 0x06, 0xd6, 0x90, 0x43, 0x39, 0xc8, 0x43, 0x39, 0x98, 0x43, 0x39,
  0xc8, 0x43, 0x39, 0xb8, 0xc3, 0x38, 0x94, 0x43, 0x38, 0x88, 0x03, 0x3b,
  0x94, 0xc3, 0x2f, 0xbc, 0x83, 0x3c, 0xfc, 0x82, 0x3b, 0xd4, 0x03, 0x3b,
  0xb0, 0xc3, 0x0c, 0xc4, 0x21, 0x07, 0x7c, 0x70, 0x03, 0x7a, 0x28, 0x87,
  0x76, 0x80, 0x87, 0x19, 0xd1, 0x43, 0x0e, 0xf8, 0xe0, 0x06, 0xe4, 0x20,
  0x0e, 0xe7, 0xe0, 0x06, 0xf6, 0x10, 0x0e, 0xf2, 0xc0, 0x0e, 0xe1, 0x90,
  0x0f, 0xef, 0x50, 0x0f, 0xf4, 0x00, 0x00, 0x00, 0x71, 0x20, 0x00, 0x00,
  0x25, 0x00, 0x00, 0x00, 0x06, 0x60, 0xbd, 0xec, 0x06, 0x48, 0x88, 0x90,
  0x22, 0x17, 0xb1, 0x94, 0x88, 0x71, 0x4c, 0x48, 0x63, 0xbc, 0x82, 0x19,
  0x6c, 0xc3, 0xe5, 0x3b, 0x8f, 0x2f, 0x04, 0x54, 0x51, 0x10, 0x51, 0xe9,
  0x00, 0x43, 0x49, 0x18, 0x80, 0x80, 0xf9, 0xc5, 0x6d, 0x1b, 0x82, 0x34,
  0x5c, 0xbe, 0xf3, 0xf8, 0x42, 0x44, 0x00, 0x13, 0x11, 0x02, 0xcd, 0xb0,
  0x10, 0x36, 0xe0, 0x0c, 0x97, 0xef, 0x3c, 0xfe, 0xe0, 0x4c, 0xb7, 0x5f,
  0xdc, 0xb6, 0x15, 0x38, 0xc3, 0xe5, 0x3b, 0x8f, 0x3f, 0x38, 0x13, 0xee,
  0x17, 0xb7, 0x6d, 0x01, 0xd3, 0x70, 0xf9, 0xce, 0xe3, 0x2f, 0x0e, 0x30,
  0x88, 0xcd, 0x43, 0x4d, 0x7e, 0x71, 0xdb, 0x26, 0x50, 0x0d, 0x97, 0xef,
  0x3c, 0xbe, 0x34, 0x39, 0x11, 0x81, 0x52, 0xd3, 0x43, 0x4d, 0x7e, 0x71,
  0xdb, 0x76, 0x20, 0x0d, 0x97, 0xef, 0x3c, 0xfe, 0x44, 0x44, 0x13, 0x02,
  0x44, 0x98, 0x5f, 0xdc, 0xb6, 0x11, 0x3c, 0xc3, 0xe5, 0x3b, 0x8f, 0x4f,
  0x35, 0x40, 0x84, 0xf9, 0xc5, 0x6d, 0x03, 0x00, 0x61, 0x20, 0x00, 0x00,
  0x32, 0x01, 0x00, 0x00, 0x13, 0x04, 0x41, 0x2c, 0x10, 0x00, 0x00, 0x00,
  0x09, 0x00, 0x00, 0x00, 0x44, 0x14, 0x57, 0x29, 0x94, 0x5d, 0x21, 0xcc,
  0x00, 0x94, 0x4c, 0xc9, 0x15, 0x4b, 0xb9, 0x14, 0x4c, 0x81, 0x94, 0x48,
  0x99, 0x14, 0x4a, 0xa9, 0x94, 0x5b, 0x71, 0x14, 0x1c, 0x15, 0x25, 0x40,
  0xd7, 0x08, 0x00, 0x00, 0x23, 0x06, 0x09, 0x00, 0x82, 0x60, 0x80, 0x91,
  0x81, 0x73, 0x89, 0x81, 0x18, 0x04, 0x23, 0x06, 0x09, 0x00, 0x82, 0x60,
  0x60, 0xb0, 0x81, 0xd4, 0x8d, 0xc1, 0x16, 0x8d, 0x18, 0x24, 0x00, 0x08,
  0x82, 0x81, 0xd1, 0x06, 0x93, 0x47, 0x06, 0x84, 0x34, 0x62, 0x90, 0x00,
  0x20, 0x08, 0x06, 0x86, 0x1b, 0x50, 0x5f, 0x19, 0x68, 0xd3, 0x88, 0x41,
  0x02, 0x80, 0x20, 0x18, 0x18, 0x6f, 0x50, 0x81, 0x81, 0x19, 0x70, 0xd4,
  0x88, 0x41, 0x02, 0x80, 0x20, 0x18, 0x18, 0x70, 0x60, 0xa1, 0xc1, 0x19,
  0x7c, 0xd5, 0x88, 0x41, 0x02, 0x80, 0x20, 0x18, 0x18, 0x71, 0x70, 0xa5,
  0x01, 0x1a, 0x20, 0xd6, 0x88, 0x41, 0x02, 0x80, 0x20, 0x18, 0x18, 0x72,
  0x80, 0xa9, 0x41, 0x1a, 0x78, 0xd7, 0x88, 0x41, 0x02, 0x80, 0x20, 0x18,
  0x18, 0x73, 0x90, 0xa9, 0x81, 0x1a, 0x88, 0x01, 0x36, 0x62, 0x90, 0x00,
  0x20, 0x08, 0x06, 0x06, 0x1d, 0x68, 0x6b, 0xb0, 0x06, 0x4b, 0x36, 0x62,
  0x90, 0x00, 0x20, 0x08, 0x06, 0x46, 0x1d, 0x6c, 0x6c, 0xc0, 0x06, 0x61,
  0xa0, 0x8d, 0x18, 0x24, 0x00, 0x08, 0x82, 0x81, 0x61, 0x07, 0x5c, 0x1b,
  0xb4, 0xc1, 0x18, 0x6c, 0x23, 0x06, 0x07, 0x00, 0x82, 0x60, 0x30, 0xc9,
  0x81, 0xc7, 0x64, 0xa3, 0x09, 0x01, 0x30, 0x9a, 0x20, 0x04, 0xa3, 0x09,
  0x83, 0x30, 0x9a, 0x40, 0x0c, 0x23, 0x06, 0x07, 0x00, 0x82, 0x60, 0x30,
  0xdd, 0xc1, 0x18, 0x44, 0xdd, 0x68, 0x42, 0x00, 0x8c, 0x26, 0x08, 0xc1,
  0x68, 0xc2, 0x20, 0x8c, 0x26, 0x10, 0xc3, 0x88, 0xc1, 0x01, 0x80, 0x20,
  0x18, 0x4c, 0x7c, 0x80, 0x06, 0x56, 0x18, 0x8c, 0x26, 0x04, 0xc0, 0x68,
  0x82, 0x10, 0x8c, 0x26, 0x0c, 0xc2, 0x68, 0x02, 0x31, 0x8c, 0x18, 0x1c,
  0x00, 0x08, 0x82, 0xc1, 0x14, 0x0a, 0x6d, 0xb0, 0xa9, 0xc1, 0x68, 0x42,
  0x00, 0x8c, 0x26, 0x08, 0xc1, 0x68, 0xc2, 0x20, 0x8c, 0x26, 0x10, 0x83,
  0x4d, 0x98, 0x7c, 0x46, 0x0c, 0x10, 0x00, 0x04, 0xc1, 0xa0, 0x32, 0x85,
  0x3a, 0xc0, 0xa6, 0x60, 0xc4, 0x00, 0x01, 0x40, 0x10, 0x0c, 0xaa, 0x53,
  0xb0, 0x03, 0x6c, 0x0a, 0x46, 0x0c, 0x10, 0x00, 0x04, 0xc1, 0xa0, 0x42,
  0x85, 0x3b, 0xc0, 0xa6, 0xc0, 0x24, 0x4e, 0x3e, 0x23, 0x06, 0x08, 0x00,
  0x82, 0x60, 0x50, 0xa9, 0x42, 0x1e, 0x70, 0x52, 0x30, 0x62, 0x80, 0x00,
  0x20, 0x08, 0x06, 0xd5, 0x2a, 0xe8, 0x01, 0x27, 0x05, 0x23, 0x06, 0x08,
  0x00, 0x82, 0x60, 0x50, 0xb1, 0xc2, 0x1e, 0x70, 0x52, 0x60, 0x11, 0x18,
  0xc8, 0x67, 0xc4, 0x00, 0x01, 0x40, 0x10, 0x0c, 0x2a, 0x57, 0xe8, 0x03,
  0x30, 0x88, 0x82, 0x11, 0x03, 0x04, 0x00, 0x41, 0x30, 0xa8, 0x5e, 0xc1,
  0x0f, 0xc0, 0x20, 0x0a, 0x46, 0x0c, 0x10, 0x00, 0x04, 0xc1, 0xa0, 0x82,
  0x85, 0x3f, 0x00, 0x83, 0x28, 0x30, 0x88, 0x0c, 0xe4, 0x33, 0x62, 0x80,
  0x00, 0x20, 0x08, 0x06, 0x95, 0x2c, 0x84, 0x02, 0x19, 0x40, 0xc1, 0x88,
  0x01, 0x02, 0x80, 0x20, 0x18, 0x54, 0xb3, 0x20, 0x0a, 0x64, 0x00, 0x05,
  0x23, 0x06, 0x08, 0x00, 0x82, 0x60, 0x50, 0xd1, 0xc2, 0x28, 0x90, 0x01,
  0x14, 0x8c, 0x18, 0x1c, 0x00, 0x08, 0x82, 0xc1, 0x64, 0x0b, 0xa2, 0x00,
  0x07, 0xad, 0x30, 0x9a, 0x10, 0x00, 0xa3, 0x09, 0x42, 0x30, 0x9a, 0x30,
  0x08, 0xa3, 0x09, 0xc4, 0x30, 0x62, 0x70, 0x00, 0x20, 0x08, 0x06, 0xd3,
  0x2e, 0x9c, 0x42, 0x1d, 0xfc, 0xc1, 0x68, 0x42, 0x00, 0x8c, 0x26, 0x08,
  0xc1, 0x68, 0xc2, 0x20, 0x8c, 0x26, 0x10, 0xc3, 0x88, 0xc1, 0x01, 0x80,
  0x20, 0x18, 0x4c, 0xe0, 0xc0, 0x0a, 0x7a, 0x30, 0x0a, 0xa3, 0x09, 0x01,
  0x30, 0x9a, 0x20, 0x04, 0xa3, 0x09, 0x83, 0x30, 0x9a, 0x40, 0x0c, 0xe6,
  0xdc, 0x81, 0x7c, 0x46, 0x0c, 0x10, 0x00, 0x04, 0xc1, 0xa0, 0x2a, 0x07,
  0x5a, 0xb8, 0x03, 0x27, 0x18, 0x31, 0x40, 0x00, 0x10, 0x04, 0x83, 0xca,
  0x1c, 0x6a, 0xe1, 0x0e, 0x9c, 0x60, 0xc4, 0x00, 0x01, 0x40, 0x10, 0x0c,
  0xaa, 0x73, 0xb0, 0x85, 0x3b, 0x70, 0x02, 0x6b, 0xf6, 0x40, 0x3e, 0x23,
  0x06, 0x08, 0x00, 0x82, 0x60, 0x50, 0xa5, 0x03, 0x2e, 0xec, 0x41, 0x13,
  0x8c, 0x18, 0x20, 0x00, 0x08, 0x82, 0x41, 0xa5, 0x0e, 0xb9, 0xb0, 0x07,
  0x4d, 0x30, 0x62, 0x80, 0x00, 0x20, 0x08, 0x06, 0xd5, 0x3a, 0xe8, 0xc2,
  0x1e, 0x34, 0x81, 0x31, 0x7f, 0x20, 0x9f, 0x11, 0x03, 0x04, 0x00, 0x41,
  0x30, 0xa8, 0xda, 0x81, 0x17, 0xfe, 0x80, 0x09, 0x46, 0x0c, 0x10, 0x00,
  0x04, 0xc1, 0xa0, 0x72, 0x87, 0x5e, 0xf8, 0x03, 0x26, 0x18, 0x31, 0x40,
  0x00, 0x10, 0x04, 0x83, 0xea, 0x1d, 0x7c, 0xe1, 0x0f, 0x98, 0x60, 0xc4,
  0xe0, 0x00, 0x40, 0x10, 0x0c, 0xa6, 0x78, 0xe8, 0x85, 0x55, 0x98, 0x85,
  0xd1, 0x84, 0x00, 0x18, 0x4d, 0x10, 0x82, 0xd1, 0x84, 0x41, 0x18, 0x31,
  0x38, 0x00, 0x10, 0x04, 0x83, 0xa9, 0x1e, 0xc2, 0xe1, 0x15, 0x6c, 0x61,
  0x34, 0x21, 0x00, 0x46, 0x13, 0x84, 0x60, 0x34, 0x61, 0x10, 0x46, 0x0c,
  0x0e, 0x00, 0x04, 0xc1, 0x60, 0xca, 0x87, 0x72, 0x98, 0x85, 0x5c, 0x18,
  0x4d, 0x08, 0x80, 0xd1, 0x04, 0x21, 0x18, 0x4d, 0x18, 0x04, 0x5b, 0x64,
  0x41, 0x3e, 0x23, 0x06, 0x08, 0x00, 0x82, 0x60, 0x50, 0xf5, 0x03, 0x3b,
  0xc8, 0xc2, 0x12, 0x8c, 0x18, 0x20, 0x00, 0x08, 0x82, 0x41, 0xe5, 0x0f,
  0xed, 0x20, 0x0b, 0x4b, 0x60, 0x4a, 0x2d, 0xc8, 0x67, 0xc4, 0x00, 0x01,
  0x40, 0x10, 0x0c, 0x2a, 0x90, 0x78, 0x87, 0x5a, 0x50, 0x82, 0x11, 0x03,
  0x04, 0x00, 0x41, 0x30, 0xa8, 0x42, 0x02, 0x1e, 0x6a, 0x41, 0x09, 0x2c,
  0xc1, 0x05, 0xf9, 0x8c, 0x18, 0x20, 0x00, 0x08, 0x82, 0x41, 0x35, 0x12,
  0xf2, 0x80, 0x0b, 0x49, 0x30, 0x62, 0x80, 0x00, 0x20, 0x08, 0x06, 0x15,
  0x49, 0xcc, 0x03, 0x2e, 0x24, 0xc1, 0x88, 0xc1, 0x02, 0x80, 0x20, 0x18,
  0x28, 0x29, 0x51, 0x0e, 0x07, 0x11, 0x1c, 0x44, 0x30, 0x62, 0x60, 0x00,
  0x20, 0x08, 0x06, 0x4c, 0x4a, 0xc0, 0x43, 0x60, 0x41, 0x22, 0x1f, 0x13,
  0x0e, 0xf9, 0xd8, 0x50, 0xc8, 0x67, 0xc4, 0xe0, 0x00, 0x40, 0x10, 0x0c,
  0xa6, 0x95, 0xb8, 0x87, 0x72, 0x48, 0x87, 0xd1, 0x84, 0x00, 0x18, 0x4d,
  0x10, 0x82, 0xd1, 0x84, 0x41, 0x18, 0x4d, 0x20, 0x86, 0x11, 0x83, 0x06,
  0x00, 0x41, 0x30, 0x70, 0x62, 0xa2, 0x1d, 0xc4, 0x21, 0x1c, 0xc0, 0xe1,
  0x17, 0x88, 0x41, 0x08, 0x46, 0x0c, 0x0c, 0x00, 0x04, 0xc1, 0x80, 0x99,
  0x09, 0x96, 0x08, 0x46, 0x0c, 0x0e, 0x00, 0x04, 0xc1, 0x60, 0x92, 0x09,
  0x7f, 0x60, 0x07, 0x97, 0x18, 0x4d, 0x08, 0x06, 0x0b, 0xd4, 0x41, 0x3e,
  0x23, 0x06, 0x09, 0x00, 0x82, 0x60, 0x80, 0xe4, 0x84, 0x48, 0xc4, 0x44,
  0x4c, 0xa4, 0x44, 0x1b, 0x8c, 0x18, 0x24, 0x00, 0x08, 0x82, 0x01, 0x92,
  0x13, 0x22, 0x11, 0x13, 0x31, 0x11, 0x0f, 0x69, 0x30, 0x62, 0x90, 0x00,
  0x20, 0x08, 0x06, 0x48, 0x4e, 0x88, 0x44, 0x4c, 0xc4, 0x84, 0x49, 0x94,
  0xc1, 0x88, 0x41, 0x02, 0x80, 0x20, 0x18, 0x20, 0x39, 0x21, 0x12, 0x31,
  0x11, 0x13, 0x27, 0x41, 0x8c, 0x18, 0x24, 0x00, 0x08, 0x82, 0x01, 0x92,
  0x13, 0x22, 0x21, 0x13, 0x31, 0x91, 0x12, 0xcd, 0x88, 0x41, 0x02, 0x80,
  0x20, 0x18, 0x20, 0x39, 0x21, 0x12, 0x32, 0x11, 0x13, 0xf1, 0xc0, 0x8c,
  0x18, 0x24, 0x00, 0x08, 0x82, 0x01, 0x92, 0x13, 0x22, 0x21, 0x13, 0x31,
  0x61, 0x12, 0xcb, 0x88, 0x41, 0x02, 0x80, 0x20, 0x18, 0x20, 0x39, 0x21,
  0x12, 0x2b, 0x11, 0x13, 0x29, 0xe1, 0x0e, 0x23, 0x06, 0x09, 0x00, 0x82,
  0x60, 0x80, 0xe4, 0x84, 0x48, 0xac, 0x44, 0x4c, 0xc4, 0x43, 0x3b, 0x8c,
  0x18, 0x24, 0x00, 0x08, 0x82, 0x01, 0x92, 0x13, 0x22, 0xb1, 0x12, 0x31,
  0x61, 0x12, 0xec, 0x30, 0x62, 0x90, 0x00, 0x20, 0x08, 0x06, 0x48, 0x4e,
  0x88, 0xc4, 0x4a, 0xc4, 0xc4, 0x49, 0x04, 0x23, 0x06, 0x09, 0x00, 0x82,
  0x60, 0x80, 0xe4, 0x84, 0x48, 0xb4, 0x44, 0x4c, 0xa4, 0x04, 0x2b, 0x8c,
  0x18, 0x24, 0x00, 0x08, 0x82, 0x01, 0x92, 0x13, 0x22, 0xd1, 0x12, 0x31,
  0x11, 0x0f, 0xa8, 0x30, 0x62, 0x90, 0x00, 0x20, 0x08, 0x06, 0x48, 0x4e,
  0x88, 0x44, 0x4b, 0xc4, 0x84, 0x49, 0x90, 0xc2, 0x88, 0x41, 0x02, 0x80,
  0x20, 0x18, 0x20, 0x39, 0x21, 0x12, 0x2d, 0x11, 0x13, 0x27, 0x01, 0x0a,
  0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};