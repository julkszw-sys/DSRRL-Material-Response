#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <d3d11_1.h>

namespace dsrrl::runtime {

// D3D11.1 command-list emulation may ignore a SetConstantBuffers1 window
// update when the same buffer remains bound at the edge of the updated range.
// Microsoft documents unbinding the affected slot before rebinding the
// explicit window as the safe workaround. Applying it to every deferred
// context is conservative and leaves immediate-context behavior unchanged.
inline void restore_ps_constant_buffer_window(
    ID3D11DeviceContext *context,
    ID3D11DeviceContext1 *context1,
    UINT slot,
    ID3D11Buffer *buffer,
    bool explicit_window,
    UINT first_constant,
    UINT num_constants) noexcept
{
    if(context==nullptr)
        return;

    if(context1!=nullptr && explicit_window){
        if(context->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED){
            ID3D11Buffer *null_buffer=nullptr;
            context->PSSetConstantBuffers(slot,1u,&null_buffer);
        }

        ID3D11Buffer *owned=buffer;
        UINT first=first_constant;
        UINT count=num_constants;
        context1->PSSetConstantBuffers1(
            slot,1u,&owned,&first,&count);
        return;
    }

    ID3D11Buffer *owned=buffer;
    context->PSSetConstantBuffers(slot,1u,&owned);
}

} // namespace dsrrl::runtime
