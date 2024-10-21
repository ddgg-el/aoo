/**
	@file
	aoo.send~ - audio meter that works by resizing an image

	@ingroup	examples
*/

#include "ext.h"							// standard Max include, always required
#include "ext_obex.h"						// required for new style Max object
#include "jpatcher_api.h"
#include "jgraphics.h"
#include "z_dsp.h"							// should be after jpatcher_api.h
#include "ext_drag.h"

#include "aoo.h"

typedef struct _aoo_send {
	t_pxjbox	p_obj;
	t_jsurface *p_surface;
	void *p_clock;
	double p_value;
	double p_max;
	char p_startclock;
} t_aoo_send;


void *aoo_send_new(t_symbol *s, long argc, t_atom *argv);
void aoo_send_free(t_aoo_send *x);
void aoo_send_assist(t_aoo_send *x, void *b, long m, long a, char *s);
void aoo_send_paint(t_aoo_send *x, t_object *patcherview);
void aoo_send_read(t_aoo_send *x, t_symbol *s);
void aoo_send_doread(t_aoo_send *x, t_symbol *s, long argc, t_atom *argv);
t_atom_long aoo_send_acceptsdrag_unlocked(t_aoo_send *x, t_object *drag, t_object *view);
void aoo_send_tick(t_aoo_send *x);
void aoo_send_perform64(t_aoo_send *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void aoo_send_dsp64(t_aoo_send *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);


static t_class *s_aoo_send_class;


void ext_main(void *r)
{
	t_class *c;

	c = class_new("aoo.send~", (method)aoo_send_new, (method)aoo_send_free, sizeof(t_aoo_send), 0L, A_GIMME, 0);

	c->c_flags |= CLASS_FLAG_NEWDICTIONARY;
	jbox_initclass(c, 0);
	class_dspinitjbox(c);

	class_addmethod(c, (method)aoo_send_dsp64,					"dsp64", A_CANT, 0);
	class_addmethod(c, (method)aoo_send_paint,					"paint",	A_CANT, 0);
	class_addmethod(c, (method)aoo_send_assist,				"assist",		A_CANT, 0);
	class_addmethod(c, (method)aoo_send_acceptsdrag_unlocked,	"acceptsdrag_unlocked", A_CANT, 0);
	class_addmethod(c, (method)aoo_send_acceptsdrag_unlocked,	"acceptsdrag_locked", A_CANT, 0);
	class_addmethod(c, (method)aoo_send_read,					"read", A_DEFSYM, 0);

	CLASS_ATTR_DEFAULT(c,"patching_rect",0, "0. 0. 128. 128.");

	class_register(CLASS_BOX, c);
	s_aoo_send_class = c;
}


void *aoo_send_new(t_symbol *s, long argc, t_atom *argv)
{
	t_aoo_send *x = NULL;
	t_dictionary *d=NULL;
	long boxflags;

	if (!(d=object_dictionaryarg(argc,argv)))
		return NULL;

	x = (t_aoo_send *)object_alloc(s_aoo_send_class);
	boxflags = 0
			   | JBOX_DRAWFIRSTIN
			   | JBOX_NODRAWBOX
			   | JBOX_DRAWINLAST
			   | JBOX_TRANSPARENT
			   //		| JBOX_NOGROW
			   //		| JBOX_GROWY
			   | JBOX_GROWBOTH
			   //		| JBOX_HILITE
			   //		| JBOX_BACKGROUND
			   //		| JBOX_DRAWBACKGROUND
			   //		| JBOX_NOFLOATINSPECTOR
			   //		| JBOX_TEXTFIELD
			   //		| JBOX_MOUSEDRAGDELTA
			   //		| JBOX_TEXTFIELD
			   ;

	jbox_new((t_jbox *)x, boxflags, argc, argv);
	x->p_obj.z_box.b_firstin = (void *)x;
	dsp_setupjbox((t_pxjbox *)x,1);
	x->p_clock = clock_new(x,(method)aoo_send_tick);
	x->p_value = x->p_max = 0;
	x->p_startclock = false;
	jbox_ready((t_jbox *)x);
	return x;
}


void aoo_send_free(t_aoo_send *x)
{
	dsp_freejbox((t_pxjbox *)x);
	freeobject((t_object *)x->p_clock);
	jbox_free((t_jbox *)x);
}


void aoo_send_assist(t_aoo_send *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_INLET)	//inlet
		sprintf(s, "(signal) Audio Input");
}


void aoo_send_paint(t_aoo_send *x, t_object *patcherview)
{
	t_rect src, dst, rect;
	t_jgraphics *g = (t_jgraphics *) patcherview_get_jgraphics(patcherview);		// obtain graphics context
	jbox_get_rect_for_view((t_object *)x, patcherview, &rect);

	if (!x->p_surface)
		return;

	// draw if the value is non zero
	if (x->p_value) {
		src.width = jgraphics_image_surface_get_width(x->p_surface);
		src.height = jgraphics_image_surface_get_height(x->p_surface);
		src.x = 0;
		src.y = 0;
		dst.width = rect.width * x->p_value;
		dst.height = rect.height * x->p_value;
		if (dst.width < 1 || dst.height < 1)
			return;
		dst.x = 0 + ((rect.width * 0.5) - (dst.width * 0.5));
		dst.y = 0 + ((rect.height * 0.5) - (dst.height * 0.5));

		jgraphics_image_surface_draw(g, x->p_surface, src, dst);
	}
}


void aoo_send_read(t_aoo_send *x, t_symbol *s)
{
	defer((t_object *)x, (method)aoo_send_doread, s, 0, NULL);
}


void aoo_send_doread(t_aoo_send *x, t_symbol *s, long argc, t_atom *argv)
{
	char filename[MAX_PATH_CHARS];
	t_fourcc *type, outtype;
	long ntype;
	t_max_err err;
	char alloc;
	short path;
	t_jsurface *surface;

	jgraphics_getfiletypes(x, &ntype, &type, &alloc);
	if (s == gensym("")) {
		err = open_dialog(filename, &path, &outtype, type, ntype);
		if (err)
			return;
	} else {
		strcpy(filename,s->s_name);
		err = locatefile_extended(filename, &path, &outtype, type, ntype);
		if (err)
			return;
	}
	surface = jgraphics_image_surface_create_referenced(filename, path);
	if (surface)
		x->p_surface = surface;
	if (alloc)
		sysmem_freeptr((char *)type);
}


t_atom_long aoo_send_acceptsdrag_unlocked(t_aoo_send *x, t_object *drag, t_object *view)
{
	if (jdrag_matchdragrole(drag, gensym("imagefile"), 0)) {
		jdrag_box_add(drag, (t_object *)x, gensym("read"));
		return true;
	}
	return false;
}


void aoo_send_tick(t_aoo_send *x)
{
	// for the astute student of the Max SDK:
	//
	// this method is called by the scheduler thread
	// x->p_max is also accessed by the perform method in the audio thread
	// we could use a mutex or critical region to protect the following block of code from having the value of x->p_max modified during its execution.
	// however, mutexes and critical regions carry a performance penalty.
	//
	// in this case, due to the nature of what we are doing (drawing something every tenth of second showing the history of the previous samples),
	// the mutex or critical region will not add anything to the object, or protect us from crashes, and it carries a performance penalty.
	// so we have made a conscious decision to not use the aforementioned thread locking mechanisms.

	if (x->p_value != x->p_max) {
		x->p_value = x->p_max;
		if (x->p_value > 1.)
			x->p_value = 1.;
		x->p_max = 0;
		jbox_redraw((t_jbox *)x);
	}

	if (sys_getdspstate())	// if the dsp is still on, schedule a next aoo_send_tick() call
		clock_fdelay(x->p_clock, 100);
}


void aoo_send_perform64(t_aoo_send *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
	t_double	*in = ins[0];
	double		xn;
	double		max;

	max = x->p_max;

	while (sampleframes--) {
		xn = *in++;
		if (xn < 0)
			xn = -xn;
		if (xn > max)
			max = xn;
	}
	x->p_max = max;

	if (x->p_startclock) {
		x->p_startclock = 0;
		clock_delay(x->p_clock,0);
	}
}


void aoo_send_dsp64(t_aoo_send *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
	x->p_value = 0.;
	x->p_max = 0.;
	// only put perf func on dsp chain if sig is connected
	if (count[0]) {
		object_method(dsp64, gensym("dsp_add64"), x, aoo_send_perform64, 0, NULL);
		x->p_startclock = true;
	}
}


