/* Same artwork and token order as the browser technique overlay. */
#include "icons.generated.h"
static void *l7_icon_pixels[9];
static void L7_DrawIcon(int index,float x,float y,float size){
 if(index<0||index>=9)return;
 const L7IconSource *source=&l7_icon_sources[index];
 if(!l7_icon_pixels[index]){
  void *allocation=calloc(source->bytes+31);if(!allocation)return;
  l7_icon_pixels[index]=(void*)(((u32)allocation+31)&~31);
  memcpy(l7_icon_pixels[index],source->pixels,source->bytes);DCFlushRange(l7_icon_pixels[index],source->bytes);
 }
 COBJ *previous=COBJ_GetCurrent();CObj_SetCurrent(event_vars->hudcam_gobj->hsd_object);
 Mtx matrix;COBJ_GetViewingMtx(COBJ_GetCurrent(),&matrix);
 HSD_ClearVtxDesc();HSD_SetupRenderMode(0x68000002);
 GXSetCurrentMtx(0);GXLoadPosMtxImm(matrix,0);
 GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
 GXSetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_F32,0);
 GXSetVtxDesc(GX_VA_POS,GX_DIRECT);GXSetVtxDesc(GX_VA_TEX0,GX_DIRECT);
 GXSetNumChans(0);GXSetNumTexGens(1);GXSetNumTevStages(1);
 GXSetTexCoordGen2(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY,0,125);
 GXSetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLOR_NULL);
 GXSetTevOp(GX_TEVSTAGE0,GX_REPLACE);
 GXSetZMode(0,GX_ALWAYS,0);GXSetCullMode(GX_CULL_NONE);
 GXSetBlendMode(GX_BM_BLEND,GX_BL_SRCALPHA,GX_BL_INVSRCALPHA,GX_LO_COPY);
 GXTexObj texture;GXInitTexObj(&texture,l7_icon_pixels[index],source->width,source->height,GX_TF_RGBA8,GX_CLAMP,GX_CLAMP,0);GXLoadTexObj(&texture,GX_TEXMAP0);
 float scale=size/(source->width>source->height?source->width:source->height);
 float width=source->width*scale,height=source->height*scale;
 GXBegin(GX_QUADS,GX_VTXFMT0,4);
 float xy[8]={x,y,x,y-height,x+width,y-height,x+width,y},uv[8]={0,0,0,1,1,1,1,0};
 for(int i=0;i<4;i++){gx_pipe->d.F32=xy[i*2];gx_pipe->d.F32=xy[i*2+1];gx_pipe->d.F32=0;gx_pipe->d.F32=uv[i*2];gx_pipe->d.F32=uv[i*2+1];}
 HSD_StateInvalidate(-1);CObj_SetCurrent(previous);
}
static void L7_IconDirection(float x,float y,int dx,int dy,float size){
 if(!dx&&!dy)return;
 COBJ *previous=COBJ_GetCurrent();CObj_SetCurrent(event_vars->hudcam_gobj->hsd_object);
 float ex=x+dx*size*.55f,ey=y+dy*size*.55f;
 GXColor white={255,255,255,255};event_vars->GFX_Start(6,(GFX_Params){.shape=GX_LINES,.size=16});
 GFX_AddVtx(x,y,0,white);GFX_AddVtx(ex,ey,0,white);
 GFX_AddVtx(ex,ey,0,white);GFX_AddVtx(ex-dx*size*.22f-dy*size*.18f,ey-dy*size*.22f+dx*size*.18f,0,white);
 GFX_AddVtx(ex,ey,0,white);GFX_AddVtx(ex-dx*size*.22f+dy*size*.18f,ey-dy*size*.22f-dx*size*.18f,0,white);
 CObj_SetCurrent(previous);
}
static void L7_DrawCue(L7SequenceAction *action,float center,float y,float size,float max_width){
 if(!action->count)return;
 float gap=0.4f,fit=(max_width-gap*(action->count-1))/action->count;
 if(size>fit)size=fit;
 float x=center-(action->count*size+(action->count-1)*gap)*0.5f;
 for(int i=0;i<action->count;i++){
  L7CueToken *token=&action->tokens[i];
  L7_DrawIcon(token->icon,x,y,size);
  L7_IconDirection(x+size*.5f,y-size*.5f,token->dx,token->dy,size);
  x+=size+gap;
 }
}
