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

NEKOC_DECLARE_HANDLE(MediaControler);
NEKOC_DECLARE_HANDLE(MediaPipeline);
NEKOC_DECLARE_HANDLE(MediaClock);
NEKOC_DECLARE_HANDLE(Pipeline);
NEKOC_DECLARE_HANDLE(Resource);
NEKOC_DECLARE_HANDLE(Graph);
NEKOC_DECLARE_HANDLE(Bus);
NEKOC_DECLARE_HANDLE(Pad);
NEKOC_DECLARE_HANDLE(Element);
NEKOC_DECLARE_HANDLE(ElementFactory);

#define neko_typeid(type) _##type##_typeid()

// Element
extern NekoElement *neko_element_create(NekoElementFactory *factory, const char *name);
extern NekoElement *neko_element_create4(NekoElementFactory *factory, NekoTypeId id);
extern void         neko_element_destroy(NekoElement *element);
extern bool         neko_element_link_with(NekoElement *source, const char *pad_name, NekoElement *sink, const char *pad2_name);


// Graph
extern NekoGraph   *neko_graph_create();
extern void         neko_graph_destroy(NekoGraph *);
extern void         neko_graph_add_element(NekoGraph *graph, NekoElement *element);
extern void         neko_graph_remove_element(NekoGraph *graph, NekoElement *element);
extern void        *neko_graph_query_interface(NekoGraph *graph, const NekoTypeId *id);

// Pipeline
extern NekoPipeline *neko_pipeline_create();
extern void          neko_pipeline_destroy(NekoPipeline *);
extern void          neko_pipeline_set_graph(NekoPipeline *pipeline, NekoGraph *graph);
extern void          neko_pipeline_set_state(NekoPipeline *pipeline, int state);

// Media
extern double        neko_media_clock_get_position(NekoMediaClock *clock);

#ifdef __cplusplus
}
#endif

#endif // _NEKOAV_CAPI_H_
