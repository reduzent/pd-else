// based on filterview

#include <stdio.h>
#include "m_pd.h"
#include "m_imp.h"
#include "g_canvas.h"

typedef struct bicoeff{
    t_object    x_obj;
    t_canvas   *x_cv;         // canvas: the widget is drawn in it
    t_glist*    x_glist;      // glist that owns the widget
    int         x_width;
    int         x_height;
    t_symbol*   x_type;
    t_symbol*   x_bind_name;  // name to bind to receive callbacks
    char        tkcanvas[MAXPDSTRING];
    char        x_tag[MAXPDSTRING];
    char        x_my[MAXPDSTRING];
}t_bicoeff;

t_class *bicoeff_class;
static t_widgetbehavior bicoeff_widgetbehavior;

// output coefficients
static void bicoeff_biquad_callback(t_bicoeff *x, t_symbol *s, int ac, t_atom* av){
    s = NULL;
    outlet_list(x->x_obj.ob_outlet, &s_list, ac, av);
}

// widgetbehavior
static void bicoeff_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2){
    t_bicoeff* x = (t_bicoeff*)z;
    *xp1 = text_xpix(&x->x_obj, glist);
    *yp1 = text_ypix(&x->x_obj, glist);
    *xp2 = text_xpix(&x->x_obj, glist) + x->x_width;
    *yp2 = text_ypix(&x->x_obj, glist) + x->x_height;
}

static void bicoeff_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_bicoeff *x = (t_bicoeff *)z;
    x->x_obj.te_xpix += dx;
    x->x_obj.te_ypix += dy;
    if(glist_isvisible(glist)){
        sys_vgui("%s move %s %d %d\n", x->tkcanvas, x->x_tag, dx, dy);
        sys_vgui("%s move RSZ %d %d\n", x->tkcanvas, dx, dy);
        canvas_fixlinesfor(glist_getcanvas(glist), (t_text*)x);
    }
}

static void bicoeff_select(t_gobj *z, t_glist *glist, int state){
    glist = NULL;
    t_bicoeff *x = (t_bicoeff *)z;
    sys_vgui("::bicoeff::select %s %d\n", x->x_my, state);
}

void bicoeff_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void bicoeff_vis(t_gobj *z, t_glist *glist, int vis){
    t_bicoeff* x = (t_bicoeff*)z;
    if(vis){
        t_canvas *cv = x->x_cv = glist_getcanvas(glist); // x_cv not used really
        snprintf(x->tkcanvas, MAXPDSTRING, ".x%lx.c", (long unsigned int)cv);
        sys_vgui("bicoeff::drawme %s %s %s %s %d %d %d %d %s\n",
            x->x_my, x->tkcanvas, x->x_bind_name->s_name, x->x_tag,
            text_xpix(&x->x_obj, glist),
            text_ypix(&x->x_obj, glist),
            text_xpix(&x->x_obj, glist)+x->x_width,
            text_ypix(&x->x_obj, glist)+x->x_height,
            x->x_type->s_name);
    }
    else
        sys_vgui("bicoeff::eraseme %s\n", x->x_my);
    // send current samplerate to the GUI for calculation of biquad coeffs
    t_float samplerate = sys_getsr();
    if(samplerate > 0)  // samplerate is sometimes 0, ignore that
        sys_vgui("set ::samplerate %.0f\n", samplerate);
    // TODO: get [block~] settings or the Tk code would not need the samplerate
}

// set filter type ----------------------------------------------------------
static void setfiltertype(t_bicoeff *x, char* type){
    x->x_type = gensym(type);
    sys_vgui("::bicoeff::setfiltertype %s %s\n", x->x_my, type);
}

static void bicoeff_allpass(t_bicoeff *x){
    setfiltertype(x, "allpass");
}

static void bicoeff_bandpass(t_bicoeff *x){
    setfiltertype(x, "bandpass");
}

static void bicoeff_highpass(t_bicoeff *x){
    setfiltertype(x, "highpass");
}

static void bicoeff_highshelf(t_bicoeff *x){
    setfiltertype(x, "highshelf");
}

static void bicoeff_lowpass(t_bicoeff *x){
    setfiltertype(x, "lowpass");
}

static void bicoeff_lowshelf(t_bicoeff *x){
    setfiltertype(x, "lowshelf");
}

static void bicoeff_notch(t_bicoeff *x){
    setfiltertype(x, "notch");
}

static void bicoeff_peaking(t_bicoeff *x){
    setfiltertype(x, "peaking");
}

static void bicoeff_resonant(t_bicoeff *x){
    setfiltertype(x, "resonant");
}

// input list for raw biquad coeffecients
static void bicoeff_list(t_bicoeff *x, t_symbol *s, int ac, t_atom *av){
    if(ac != 5)
        pd_error(x, "[bicoeff] needs 5 float coefficients, ignoring list");
    else{
        t_float a1 = atom_getfloat(av);
        t_float a2 = atom_getfloat(av + 1);
        t_float b0 = atom_getfloat(av + 2);
        t_float b1 = atom_getfloat(av + 3);
        t_float b2 = atom_getfloat(av + 4);
//        setfiltertype(x, "raw");
        sys_vgui("::bicoeff::coefficients %s %g %g %g %g %g\n", x->x_my, a1, a2, b0, b1, b2);
        bicoeff_biquad_callback(x, s, ac, av);
    }
}

/*static int bicoeff_click(t_bicoeff *x, t_glist *gl, int xpos,int ypos, int shift, int alt,
    int dbl, int doit)
{
    
}*/

// object and class creation/destruction -----------------------------------
static void *bicoeff_new(t_symbol *s, int ac, t_atom* av){
    s = NULL;
    t_bicoeff *x = (t_bicoeff *)pd_new(bicoeff_class);
    int width = 450;
    int height = 150;
    if(ac == 2){
        width = atom_getfloatarg(0, ac, av);
        height = atom_getfloatarg(1, ac, av);
    }
    x->x_width = width;
    x->x_height = height;
    x->x_type = gensym("peaking");
    x->x_glist = canvas_getcurrent();
    snprintf(x->x_tag, MAXPDSTRING, "T%lx", (long unsigned int)x);
    snprintf(x->x_my, MAXPDSTRING, "::N%lx", (long unsigned int)x);
    char buf[MAXPDSTRING];
    sprintf(buf, "#R%lx", (long unsigned int)x);
    x->x_bind_name = gensym(buf);
    pd_bind(&x->x_obj.ob_pd, x->x_bind_name);
    outlet_new(&x->x_obj, &s_list);
    return(void *)x;
}

static void bicoeff_free(t_bicoeff *x){
    pd_unbind(&x->x_obj.ob_pd, x->x_bind_name);
}

void bicoeff_setup(void){
    bicoeff_class = class_new(gensym("bicoeff"), (t_newmethod)bicoeff_new,
        (t_method)bicoeff_free, sizeof(t_bicoeff), 0, A_GIMME, 0);
    class_addlist(bicoeff_class, (t_method)bicoeff_list);
    class_addmethod(bicoeff_class, (t_method)bicoeff_allpass, gensym("allpass"), 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_bandpass, gensym("bandpass"), 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_highpass, gensym("highpass"), 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_highshelf, gensym("highshelf"), 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_lowpass, gensym("lowpass"), 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_lowshelf, gensym("lowshelf"), 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_notch, gensym("bandstop"), 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_peaking, gensym("eq"), 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_resonant, gensym("resonant"), 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_biquad_callback, gensym("biquad"), A_GIMME, 0);
    // widget behavior
    bicoeff_widgetbehavior.w_getrectfn  = bicoeff_getrect;
    bicoeff_widgetbehavior.w_displacefn = bicoeff_displace;
    bicoeff_widgetbehavior.w_selectfn   = bicoeff_select;
    bicoeff_widgetbehavior.w_activatefn = NULL;
    bicoeff_widgetbehavior.w_deletefn   = bicoeff_delete;
    bicoeff_widgetbehavior.w_visfn      = bicoeff_vis;
//    bicoeff_widgetbehavior.w_clickfn    = (t_clickfn)bicoeff_click;
    bicoeff_widgetbehavior.w_clickfn    = NULL;
    class_setwidget(bicoeff_class, &bicoeff_widgetbehavior);
//    class_setsavefn(bicoeff_class, &bicoeff_save);
    sys_vgui("eval [read [open {%s/bicoeff.tcl}]]\n", bicoeff_class->c_externdir->s_name);
}
