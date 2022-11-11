// porres

#include "m_pd.h"
#include "shared/buffer.h"

typedef struct _tabreader{
    t_object  x_obj;
    t_buffer *x_buffer;
    int       x_i_mode;
    int       x_ch;
    t_float   x_bias;
    t_float   x_tension;
    t_float   x_value;
    t_outlet *x_outlet;
}t_tabreader;

static t_class *tabreader_class;

static void tabreader_set_nointerp(t_tabreader *x){
    x->x_i_mode = 0;
}

static void tabreader_set_linear(t_tabreader *x){
    x->x_i_mode = 1;
}

static void tabreader_set_sin(t_tabreader *x){
    x->x_i_mode = 2;
}

static void tabreader_set_lagrange(t_tabreader *x){
    x->x_i_mode = 3;
}

static void tabreader_set_cubic(t_tabreader *x){
    x->x_i_mode = 4;
}

static void tabreader_set_spline(t_tabreader *x){
    x->x_i_mode = 5;
}

static void tabreader_set_hermite(t_tabreader *x, t_floatarg bias, t_floatarg tension){
    x->x_bias = bias;
    x->x_tension = 0.5 * (1. - tension);
    x->x_i_mode = 6;
}

static void tabreader_set(t_tabreader *x, t_symbol *s){
    buffer_setarray(x->x_buffer, s);
}

static void tabreader_float(t_tabreader *x, t_float f){
    t_buffer *buf = x->x_buffer;
    t_word *vp = buf->c_vectors[0];
    int npts = buf->c_npts;
    buffer_validate(buf, 1); // 2nd arg for error posting
    if(vp){
        double phase = (double)(f);
        double xpos = phase*npts;
        if(phase < 0)
            phase = 0;
        else if(phase >= 1.0)
            phase = 0;
        int ndx = (int)xpos;
        double frac = xpos - ndx;
        if(ndx == npts)
            ndx = 0;
        int ndx1 = ndx + 1;
        if(ndx1 == npts)
            ndx1 = 0;
        int ndxm1 = 0, ndx2 = 0;
        if(x->x_i_mode){
            ndxm1 = ndx - 1;
            if(ndxm1 < 0)
                ndxm1 = npts - 1;
            ndx2 = ndx1 + 1;
            if(ndx2 >= npts)
                ndx2 -= npts;
        }
        double a = 0, b = 0, c = 0, d = 0;
        b = (double)vp[ndx].w_float;
        if(x->x_i_mode){
            c = (double)vp[ndx1].w_float;
            if(x->x_i_mode > 2){
                a = (double)vp[ndxm1].w_float;
                d = (double)vp[ndx2].w_float;
            }
        }
//        post("a (%f) b (%f) c (%f) d (%f)", (float)a, (float)b, (float)c, (float)d);
        float out = b; // no interpolation
        switch(x->x_i_mode){
            case 1: // linear
                out = interp_lin(frac, b, c);
                break;
            case 2: // sin
                out = interp_sin(frac, b, c);
                break;
            case 3: // lagrange
                out = interp_lagrange(frac, a, b, c, d);
                break;
            case 4: // cubic
                out = interp_cubic(frac, a, b, c, d);
                break;
            case 5: // spline
                out = interp_spline(frac, a, b, c, d);
                break;
            case 6: // hermite
                out = interp_hermite(frac, a, b, c, d, x->x_bias, x->x_tension);
                break;
            default:
                break;
        }
        outlet_float(x->x_outlet, out);
    }
}

static void tabreader_channel(t_tabreader *x, t_floatarg f){
    x->x_ch = f < 1 ? 1 : (f > 64 ? 64 : (int) f);
    buffer_getchannel(x->x_buffer, x->x_ch, 1);
}

static void tabreader_free(t_tabreader *x){
    outlet_free(x->x_outlet);
    buffer_free(x->x_buffer);
}

static void *tabreader_new(t_symbol *s, int ac, t_atom * av){
    t_tabreader *x = (t_tabreader *)pd_new(tabreader_class);
    t_symbol *name = s = NULL;
    int nameset = 0;
    x->x_bias = x->x_tension = 0;
    x->x_i_mode = 3; // lagrange
    int ch = 1;
    while(ac){
        if(av->a_type == A_SYMBOL){ // symbol
            t_symbol *curarg = atom_getsymbol(av);
            if(curarg == gensym("-none")){
                if(nameset)
                    goto errstate;
                x->x_i_mode = 0, ac--, av++;
            }
            else if(curarg == gensym("-lin")){
                if(nameset)
                    goto errstate;
                tabreader_set_linear(x), ac--, av++;
            }
            else if(curarg == gensym("-sin")){
                if(nameset)
                    goto errstate;
                tabreader_set_sin(x), ac--, av++;
            }
            else if(curarg == gensym("-cubic")){
                if(nameset)
                    goto errstate;
                tabreader_set_cubic(x), ac--, av++;
            }
            else if(curarg == gensym("-spline")){
                if(nameset)
                    goto errstate;
                tabreader_set_spline(x), ac--, av++;
            }
            else if(curarg == gensym("-hermite")){
                if(nameset)
                    goto errstate;
                if(ac >= 3){
                    x->x_i_mode = 5, ac--, av++;
                    float bias = atom_getfloat(av);
                    ac--, av++;
                    float tension = atom_getfloat(av);
                    ac--, av++;
                    tabreader_set_hermite(x, bias, tension);
                }
                else
                    goto errstate;
            }
            else{
                if(nameset)
                    goto errstate;
                name = atom_getsymbol(av);
                nameset = 1, ac--, av++;
            }
        }
        else
            ch = (int)atom_getfloat(av), ac--, av++;
    };
    x->x_ch = (ch < 0 ? 1 : ch > 64 ? 64 : ch);
    x->x_buffer = buffer_init((t_class *)x, name, 1, x->x_ch);
    buffer_getchannel(x->x_buffer, x->x_ch, 1);
    buffer_setminsize(x->x_buffer, 2);
    buffer_playcheck(x->x_buffer);
    x->x_outlet = outlet_new(&x->x_obj, &s_float);
    return(x);
    errstate:
        post("tabreader: improper args");
        return(NULL);
}

void tabreader_setup(void){
    tabreader_class = class_new(gensym("tabreader"), (t_newmethod)tabreader_new, (t_method)tabreader_free,
        sizeof(t_tabreader), 0, A_GIMME, 0);
    class_addfloat(tabreader_class, tabreader_float);
    class_addmethod(tabreader_class, (t_method)tabreader_set, gensym("set"), A_SYMBOL, 0);
    class_addmethod(tabreader_class, (t_method)tabreader_channel, gensym("channel"), A_FLOAT, 0);
    class_addmethod(tabreader_class, (t_method)tabreader_set_nointerp, gensym("none"), 0);
    class_addmethod(tabreader_class, (t_method)tabreader_set_linear, gensym("lin"), 0);
    class_addmethod(tabreader_class, (t_method)tabreader_set_lagrange, gensym("lagrange"), 0);
    class_addmethod(tabreader_class, (t_method)tabreader_set_cubic, gensym("cubic"), 0);
    class_addmethod(tabreader_class, (t_method)tabreader_set_spline, gensym("spline"), 0);
    class_addmethod(tabreader_class, (t_method)tabreader_set_sin, gensym("sin"), 0);
    class_addmethod(tabreader_class, (t_method)tabreader_set_hermite, gensym("hermite"),
        A_FLOAT, A_FLOAT, 0);
}
