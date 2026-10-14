"""Synthetic CPU decoder guards; no PTDE or DSR retail bytes bundled."""
from dsr_stable_ls_tail_cpu import decode_operand,source,execute,decode_instructions
def expect_error(f):
 try:f()
 except (ValueError,KeyError,AssertionError):return
 raise AssertionError("malformed DXBC accepted")
def test():
 op,n=decode_operand([0x8010001a,0x41,0],0)
 assert n==3 and op[0]==0 and op[-1]==1
 assert source(op,{("r",0):[1.,2.,3.,4.]})==[-2.]*4
 cb,n=decode_operand([0x0020802a,0,16],0)
 assert n==3 and source(cb,{("cb",0,16):[10.,20.,30.,40.]})==[30.]*4
 imm,n=decode_operand([0x00004001,0x3f800000],0)
 assert n==2 and source(imm,{})==[1.]*4
 dst=(0,(1,),2,0,7,0)
 temp=(0,(2,),2,1,0xe4,0)
 constant=(8,(0,14),2,1,0xe4,0)
 state={("r",1):[0.,0.,0.,9.],("r",2):[1.,2.,3.,4.],
        ("cb",0,14):[2.,3.,4.,5.]}
 out=execute([(2,0x38,[dst,temp,constant])],state)
 assert out[("r",1)]==[2.,6.,12.,9.]
 assert out[("r",2)]==[1.,2.,3.,4.]
 expect_error(lambda:decode_operand([0x00500072,0],0))
 expect_error(lambda:decode_instructions([0,0,0x010003ff],2,3))
 print("V295_DSR_TOKEN_OPERAND_AND_SCATTERING_CPU_GUARDS_PASS")
if __name__=="__main__":test()
