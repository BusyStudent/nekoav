#if !defined(_NEKOAV_CAPI_H_)
#define _NEKOAV_CAPI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#define NEKOC_DECLARE_HANDLE(name) \
    typedef struct _Neko##name * Neko##name; \
    extern NekoTypeId _Neko##name##_typeid();

typedef const struct _NekoTypeId *NekoTypeId;

NEKOC_DECLARE_HANDLE(MediaClock);
NEKOC_DECLARE_HANDLE(MediaFrame);
NEKOC_DECLARE_HANDLE(Pipeline);
NEKOC_DECLARE_HANDLE(Resource);
NEKOC_DECLARE_HANDLE(Bus);
NEKOC_DECLARE_HANDLE(Pad);
NEKOC_DECLARE_HANDLE(Element);
NEKOC_DECLARE_HANDLE(ElementFactory);
NEKOC_DECLARE_HANDLE(Player);

using NekoClock = NekoMediaClock;
using NekoFrame = NekoMediaFrame;

#define neko_typeid(type) _##type##_typeid()

// Element
extern NekoElement *neko_element_create(NekoElementFactory *factory, const char *name);
extern NekoElement *neko_element_create4(NekoElementFactory *factory, NekoTypeId id);
extern void         neko_element_destroy(NekoElement *element);
extern bool         neko_element_link_with(NekoElement *source, const char *pad_name, NekoElement *sink, const char *pad2_name);

// Pipeline
extern NekoPipeline *neko_pipeline_create();
extern void          neko_pipeline_destroy(NekoPipeline *);
extern void          neko_pipeline_set_state(NekoPipeline *pipeline, int state);

// Media
extern double        neko_clock_get_position(NekoClock *clock);
extern void          neko_frame_destroy(NekoFrame *frame);
extern int           neko_frame_get_linesize(NekoFrame *frame, int plane);
extern void *        neko_frame_get_data(NekoFrame *frame, int plane);
extern int           neko_frame_get_width(NekoFrame *frame);
extern int           neko_frame_get_height(NekoFrame *frame);

// Player
extern NekoPlayer    *neko_player_create();
extern void           neko_player_destroy(NekoPlayer *player);
extern void           neko_player_set_url(NekoPlayer *player, const char *url);
extern void           neko_player_play(NekoPlayer *player);
extern void           neko_player_pause(NekoPlayer *player);
extern void           neko_player_stop(NekoPlayer *player);
extern void           neko_player_set_positon_callback(NekoPlayer *player, void (*callback)(double, void *), void *);



#ifdef __cplusplus
}
#endif

#endif // _NEKOAV_CAPI_H_
