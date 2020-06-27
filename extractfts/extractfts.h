#ifndef EXTRACT_FTS_H
#define EXTRACT_FTS_H

#include "libavutil/buffer.h"
#include "libavutil/intreadwrite.h"

#include "h264.h"
#include "h264dsp.h"
#include "h264chroma.h"
#include "h264pred.h"
#include "qpeldsp.h"
#include "h264qpel.h"
#include "h264_sei.h"
#include "videodsp.h"

typedef          int       atomic_int;

typedef struct ThreadFrame {
	AVFrame *f;
	AVCodecContext *owner[2];
	// progress->data is an array of 2 ints holding progress for top/bottom
	// fields
	AVBufferRef *progress;
} ThreadFrame;


#define H264_MAX_PICTURE_COUNT 36

#define MAX_MMCO_COUNT         66

#define MAX_DELAYED_PIC_COUNT  16

/* Compiling in interlaced support reduces the speed
 * of progressive decoding by about 2%. */
#define ALLOW_INTERLACE

#define FMO 0

 /**
  * The maximum number of slices supported by the decoder.
  * must be a power of 2
  */
#define MAX_SLICES 32

#ifdef ALLOW_INTERLACE
#define MB_MBAFF(h)    (h)->mb_mbaff
#define MB_FIELD(sl)  (sl)->mb_field_decoding_flag
#define FRAME_MBAFF(h) (h)->mb_aff_frame
#define FIELD_PICTURE(h) ((h)->picture_structure != PICT_FRAME)
#define LEFT_MBS 2
#define LTOP     0
#define LBOT     1
#define LEFT(i)  (i)
#else
#define MB_MBAFF(h)      0
#define MB_FIELD(sl)     0
#define FRAME_MBAFF(h)   0
#define FIELD_PICTURE(h) 0
#undef  IS_INTERLACED
#define IS_INTERLACED(mb_type) 0
#define LEFT_MBS 1
#define LTOP     0
#define LBOT     0
#define LEFT(i)  0
#endif
#define FIELD_OR_MBAFF_PICTURE(h) (FRAME_MBAFF(h) || FIELD_PICTURE(h))

#ifndef CABAC
#define CABAC(h) (h)->ps.pps->cabac
#endif

#define CHROMA(h)    ((h)->ps.sps->chroma_format_idc)
#define CHROMA422(h) ((h)->ps.sps->chroma_format_idc == 2)
#define CHROMA444(h) ((h)->ps.sps->chroma_format_idc == 3)

#define MB_TYPE_REF0       MB_TYPE_ACPRED // dirty but it fits in 16 bit
#define MB_TYPE_8x8DCT     0x01000000
#define IS_REF0(a)         ((a) & MB_TYPE_REF0)
#define IS_8x8DCT(a)       ((a) & MB_TYPE_8x8DCT)

  /**
   * Memory management control operation opcode.
   */
typedef enum MMCOOpcode {
	MMCO_END = 0,
	MMCO_SHORT2UNUSED,
	MMCO_LONG2UNUSED,
	MMCO_SHORT2LONG,
	MMCO_SET_MAX_LONG,
	MMCO_RESET,
	MMCO_LONG,
} MMCOOpcode;

/**
 * Memory management control operation.
 */
typedef struct MMCO {
	MMCOOpcode opcode;
	int short_pic_num;  ///< pic_num without wrapping (pic_num & max_pic_num)
	int long_arg;       ///< index, pic_num, or num long refs depending on opcode
} MMCO;

///////

#define CACHED_BITSTREAM_READER !ARCH_X86_32
#define UNCHECKED_BITSTREAM_READER 1

#define MAX_MBPAIR_SIZE (256*1024) // a tighter bound could be calculated if someone cares about a few bytes


typedef struct GetBitContext {
	const uint8_t *buffer, *buffer_end;
#if CACHED_BITSTREAM_READER
	uint64_t cache;
	unsigned bits_left;
#endif
	int index;
	int size_in_bits;
	int size_in_bits_plus8;
} GetBitContext;

typedef struct H2645NAL {
	uint8_t *rbsp_buffer;

	int size;
	const uint8_t *data;

	/**
	 * Size, in bits, of just the data, excluding the stop bit and any trailing
	 * padding. I.e. what HEVC calls SODB.
	 */
	int size_bits;

	int raw_size;
	const uint8_t *raw_data;

	GetBitContext gb;

	/**
	 * NAL unit type
	 */
	int type;

	/**
	 * HEVC only, nuh_temporal_id_plus_1 - 1
	 */
	int temporal_id;

	int skipped_bytes;
	int skipped_bytes_pos_size;
	int *skipped_bytes_pos;
	/**
	 * H.264 only, nal_ref_idc
	 */
	int ref_idc;
} H2645NAL;

typedef struct H2645RBSP {
	uint8_t *rbsp_buffer;
	AVBufferRef *rbsp_buffer_ref;
	int rbsp_buffer_alloc_size;
	int rbsp_buffer_size;
} H2645RBSP;

/* an input packet split into unescaped NAL units */
typedef struct H2645Packet {
	H2645NAL *nals;
	H2645RBSP rbsp;
	int nb_nals;
	int nals_allocated;
} H2645Packet;

///////

//////

#define MAX_SPS_COUNT          32
#define MAX_PPS_COUNT         256
#define MAX_LOG2_MAX_FRAME_NUM    (12 + 4)

/**
 * Sequence parameter set
 */
typedef struct SPS {
	unsigned int sps_id;
	int profile_idc;
	int level_idc;
	int chroma_format_idc;
	int transform_bypass;              ///< qpprime_y_zero_transform_bypass_flag
	int log2_max_frame_num;            ///< log2_max_frame_num_minus4 + 4
	int poc_type;                      ///< pic_order_cnt_type
	int log2_max_poc_lsb;              ///< log2_max_pic_order_cnt_lsb_minus4
	int delta_pic_order_always_zero_flag;
	int offset_for_non_ref_pic;
	int offset_for_top_to_bottom_field;
	int poc_cycle_length;              ///< num_ref_frames_in_pic_order_cnt_cycle
	int ref_frame_count;               ///< num_ref_frames
	int gaps_in_frame_num_allowed_flag;
	int mb_width;                      ///< pic_width_in_mbs_minus1 + 1
	///< (pic_height_in_map_units_minus1 + 1) * (2 - frame_mbs_only_flag)
	int mb_height;
	int frame_mbs_only_flag;
	int mb_aff;                        ///< mb_adaptive_frame_field_flag
	int direct_8x8_inference_flag;
	int crop;                          ///< frame_cropping_flag

	/* those 4 are already in luma samples */
	unsigned int crop_left;            ///< frame_cropping_rect_left_offset
	unsigned int crop_right;           ///< frame_cropping_rect_right_offset
	unsigned int crop_top;             ///< frame_cropping_rect_top_offset
	unsigned int crop_bottom;          ///< frame_cropping_rect_bottom_offset
	int vui_parameters_present_flag;
	AVRational sar;
	int video_signal_type_present_flag;
	int full_range;
	int colour_description_present_flag;
	enum AVColorPrimaries color_primaries;
	enum AVColorTransferCharacteristic color_trc;
	enum AVColorSpace colorspace;
	int timing_info_present_flag;
	uint32_t num_units_in_tick;
	uint32_t time_scale;
	int fixed_frame_rate_flag;
	int32_t offset_for_ref_frame[256];
	int bitstream_restriction_flag;
	int num_reorder_frames;
	int scaling_matrix_present;
	uint8_t scaling_matrix4[6][16];
	uint8_t scaling_matrix8[6][64];
	int nal_hrd_parameters_present_flag;
	int vcl_hrd_parameters_present_flag;
	int pic_struct_present_flag;
	int time_offset_length;
	int cpb_cnt;                          ///< See H.264 E.1.2
	int initial_cpb_removal_delay_length; ///< initial_cpb_removal_delay_length_minus1 + 1
	int cpb_removal_delay_length;         ///< cpb_removal_delay_length_minus1 + 1
	int dpb_output_delay_length;          ///< dpb_output_delay_length_minus1 + 1
	int bit_depth_luma;                   ///< bit_depth_luma_minus8 + 8
	int bit_depth_chroma;                 ///< bit_depth_chroma_minus8 + 8
	int residual_color_transform_flag;    ///< residual_colour_transform_flag
	int constraint_set_flags;             ///< constraint_set[0-3]_flag
	uint8_t data[4096];
	size_t data_size;
} SPS;

/**
 * Picture parameter set
 */
typedef struct PPS {
	unsigned int sps_id;
	int cabac;                  ///< entropy_coding_mode_flag
	int pic_order_present;      ///< pic_order_present_flag
	int slice_group_count;      ///< num_slice_groups_minus1 + 1
	int mb_slice_group_map_type;
	unsigned int ref_count[2];  ///< num_ref_idx_l0/1_active_minus1 + 1
	int weighted_pred;          ///< weighted_pred_flag
	int weighted_bipred_idc;
	int init_qp;                ///< pic_init_qp_minus26 + 26
	int init_qs;                ///< pic_init_qs_minus26 + 26
	int chroma_qp_index_offset[2];
	int deblocking_filter_parameters_present; ///< deblocking_filter_parameters_present_flag
	int constrained_intra_pred;     ///< constrained_intra_pred_flag
	int redundant_pic_cnt_present;  ///< redundant_pic_cnt_present_flag
	int transform_8x8_mode;         ///< transform_8x8_mode_flag
	uint8_t scaling_matrix4[6][16];
	uint8_t scaling_matrix8[6][64];
	uint8_t chroma_qp_table[2][QP_MAX_NUM + 1];  ///< pre-scaled (with chroma_qp_index_offset) version of qp_table
	int chroma_qp_diff;
	uint8_t data[4096];
	size_t data_size;

	uint32_t dequant4_buffer[6][QP_MAX_NUM + 1][16];
	uint32_t dequant8_buffer[6][QP_MAX_NUM + 1][64];
	uint32_t(*dequant4_coeff[6])[16];
	uint32_t(*dequant8_coeff[6])[64];
} PPS;

typedef struct H264ParamSets {
	AVBufferRef *sps_list[MAX_SPS_COUNT];
	AVBufferRef *pps_list[MAX_PPS_COUNT];

	AVBufferRef *pps_ref;
	AVBufferRef *sps_ref;
	/* currently active parameters sets */
	const PPS *pps;
	const SPS *sps;
} H264ParamSets;

///////

typedef struct H264PredWeightTable {
	int use_weight;
	int use_weight_chroma;
	int luma_log2_weight_denom;
	int chroma_log2_weight_denom;
	int luma_weight_flag[2];    ///< 7.4.3.2 luma_weight_lX_flag
	int chroma_weight_flag[2];  ///< 7.4.3.2 chroma_weight_lX_flag
	// The following 2 can be changed to int8_t but that causes a 10 CPU cycles speed loss
	int luma_weight[48][2][2];
	int chroma_weight[48][2][2][2];
	int implicit_weight[48][48][2];
} H264PredWeightTable;

typedef struct H264POCContext {
	int poc_lsb;
	int poc_msb;
	int delta_poc_bottom;
	int delta_poc[2];
	int frame_num;
	int prev_poc_msb;           ///< poc_msb of the last reference pic for POC type 0
	int prev_poc_lsb;           ///< poc_lsb of the last reference pic for POC type 0
	int frame_num_offset;       ///< for POC type 2
	int prev_frame_num_offset;  ///< for POC type 2
	int prev_frame_num;         ///< frame_num of the last pic for POC type 1/2
} H264POCContext;

///////


extern const uint32_t ff_square_tab[512];


/* minimum alignment rules ;)
 * If you notice errors in the align stuff, need more alignment for some ASM code
 * for some CPU or need to use a function with less aligned data then send a mail
 * to the ffmpeg-devel mailing list, ...
 *
 * !warning These alignments might not match reality, (missing attribute((align))
 * stuff somewhere possible).
 * I (Michael) did not check them, these are just the alignments which I think
 * could be reached easily ...
 *
 * !future video codecs might need functions with less strict alignment
 */

struct MpegEncContext;
/* Motion estimation:
 * h is limited to { width / 2, width, 2 * width },
 * but never larger than 16 and never smaller than 2.
 * Although currently h < 4 is not used as functions with
 * width < 8 are neither used nor implemented. */
typedef int(*me_cmp_func)(struct MpegEncContext *c,
	uint8_t *blk1 /* align width (8 or 16) */,
	uint8_t *blk2 /* align 1 */, ptrdiff_t stride,
	int h);

typedef struct MECmpContext {
	int(*sum_abs_dctelem)(int16_t *block /* align 16 */);

	me_cmp_func sad[6]; /* identical to pix_absAxA except additional void * */
	me_cmp_func sse[6];
	me_cmp_func hadamard8_diff[6];
	me_cmp_func dct_sad[6];
	me_cmp_func quant_psnr[6];
	me_cmp_func bit[6];
	me_cmp_func rd[6];
	me_cmp_func vsad[6];
	me_cmp_func vsse[6];
	me_cmp_func nsse[6];
	me_cmp_func w53[6];
	me_cmp_func w97[6];
	me_cmp_func dct_max[6];
	me_cmp_func dct264_sad[6];

	me_cmp_func me_pre_cmp[6];
	me_cmp_func me_cmp[6];
	me_cmp_func me_sub_cmp[6];
	me_cmp_func mb_cmp[6];
	me_cmp_func ildct_cmp[6]; // only width 16 used
	me_cmp_func frame_skip_cmp[6]; // only width 8 used

	me_cmp_func pix_abs[2][4];
	me_cmp_func median_sad[6];
} MECmpContext;


///< current MB is the first after a resync marker
#define VP_START               1
#define ER_AC_ERROR            2
#define ER_DC_ERROR            4
#define ER_MV_ERROR            8
#define ER_AC_END              16
#define ER_DC_END              32
#define ER_MV_END              64

#define ER_MB_ERROR (ER_AC_ERROR|ER_DC_ERROR|ER_MV_ERROR)
#define ER_MB_END   (ER_AC_END|ER_DC_END|ER_MV_END)

typedef struct ERPicture {
	AVFrame *f;
	ThreadFrame *tf;

	// it is the caller's responsibility to allocate these buffers
	int16_t(*motion_val[2])[2];
	int8_t *ref_index[2];

	uint32_t *mb_type;
	int field_picture;
} ERPicture;

typedef struct ERContext {
	AVCodecContext *avctx;
	MECmpContext mecc;
	int mecc_inited;

	int *mb_index2xy;
	int mb_num;
	int mb_width, mb_height;
	ptrdiff_t mb_stride;
	ptrdiff_t b8_stride;

	atomic_int error_count;
	int error_occurred;
	uint8_t *error_status_table;
	uint8_t *er_temp_buffer;
	int16_t *dc_val[3];
	uint8_t *mbskip_table;
	uint8_t *mbintra_table;
	int mv[2][4][2];

	ERPicture cur_pic;
	ERPicture last_pic;
	ERPicture next_pic;

	AVBufferRef *ref_index_buf[2];
	AVBufferRef *motion_val_buf[2];

	uint16_t pp_time;
	uint16_t pb_time;
	int quarter_sample;
	int partitioned_frame;
	int ref_count;

	void(*decode_mb)(void *opaque, int ref, int mv_dir, int mv_type,
		int(*mv)[2][4][2],
		int mb_x, int mb_y, int mb_intra, int mb_skipped);
	void *opaque;
} ERContext;

///////
typedef struct PutBitContext {
	uint32_t bit_buf;
	int bit_left;
	uint8_t *buf, *buf_ptr, *buf_end;
	int size_in_bits;
} PutBitContext;

extern const uint8_t ff_h264_cabac_tables[512 + 4 * 2 * 64 + 4 * 64 + 63];
#define H264_NORM_SHIFT_OFFSET 0
#define H264_LPS_RANGE_OFFSET 512
#define H264_MLPS_STATE_OFFSET 1024
#define H264_LAST_COEFF_FLAG_OFFSET_8x8_OFFSET 1280

#define CABAC_BITS 16
#define CABAC_MASK ((1<<CABAC_BITS)-1)

typedef struct CABACContext {
	int low;
	int range;
	int outstanding_count;
	const uint8_t *bytestream_start;
	const uint8_t *bytestream;
	const uint8_t *bytestream_end;
	PutBitContext pb;
}CABACContext;
///////
typedef struct H264Picture {
	AVFrame *f;
	ThreadFrame tf;

	AVBufferRef *qscale_table_buf;
	int8_t *qscale_table;

	AVBufferRef *motion_val_buf[2];
	int16_t(*motion_val[2])[2];

	AVBufferRef *mb_type_buf;
	uint32_t *mb_type;

	AVBufferRef *hwaccel_priv_buf;
	void *hwaccel_picture_private; ///< hardware accelerator private data

	AVBufferRef *ref_index_buf[2];
	int8_t *ref_index[2];

	int field_poc[2];       ///< top/bottom POC
	int poc;                ///< frame POC
	int frame_num;          ///< frame_num (raw frame_num from slice header)
	int mmco_reset;         /**< MMCO_RESET set this 1. Reordering code must
								 not mix pictures before and after MMCO_RESET. */
	int pic_id;             /**< pic_num (short -> no wrap version of pic_num,
								 pic_num & max_pic_num; long -> long_pic_num) */
	int long_ref;           ///< 1->long term reference 0->short term reference
	int ref_poc[2][2][32];  ///< POCs of the frames/fields used as reference (FIXME need per slice)
	int ref_count[2][2];    ///< number of entries in ref_poc         (FIXME need per slice)
	int mbaff;              ///< 1 -> MBAFF frame 0-> not MBAFF
	int field_picture;      ///< whether or not picture was encoded in separate fields

	int reference;
	int recovered;          ///< picture at IDR or recovery point + recovery count
	int invalid_gap;
	int sei_recovery_frame_cnt;
} H264Picture;

typedef struct H264Ref {
	uint8_t *data[3];
	int linesize[3];

	int reference;
	int poc;
	int pic_id;

	H264Picture *parent;
} H264Ref;


typedef struct H264SliceContext {
	struct H264Context *h264;
	GetBitContext gb;
	ERContext er;

	int slice_num;
	int slice_type;
	int slice_type_nos;         ///< S free slice type (SI/SP are remapped to I/P)
	int slice_type_fixed;

	int qscale;
	int chroma_qp[2];   // QPc
	int qp_thresh;      ///< QP threshold to skip loopfilter
	int last_qscale_diff;

	// deblock
	int deblocking_filter;          ///< disable_deblocking_filter_idc with 1 <-> 0
	int slice_alpha_c0_offset;
	int slice_beta_offset;

	H264PredWeightTable pwt;

	int prev_mb_skipped;
	int next_mb_skipped;

	int chroma_pred_mode;
	int intra16x16_pred_mode;

	int8_t intra4x4_pred_mode_cache[5 * 8];
	int8_t(*intra4x4_pred_mode);

	int topleft_mb_xy;
	int top_mb_xy;
	int topright_mb_xy;
	int left_mb_xy[LEFT_MBS];

	int topleft_type;
	int top_type;
	int topright_type;
	int left_type[LEFT_MBS];

	const uint8_t *left_block;
	int topleft_partition;

	unsigned int topleft_samples_available;
	unsigned int top_samples_available;
	unsigned int topright_samples_available;
	unsigned int left_samples_available;

	ptrdiff_t linesize, uvlinesize;
	ptrdiff_t mb_linesize;  ///< may be equal to s->linesize or s->linesize * 2, for mbaff
	ptrdiff_t mb_uvlinesize;

	int mb_x, mb_y;
	int mb_xy;
	int resync_mb_x;
	int resync_mb_y;
	unsigned int first_mb_addr;
	// index of the first MB of the next slice
	int next_slice_idx;
	int mb_skip_run;
	int is_complex;

	int picture_structure;
	int mb_field_decoding_flag;
	int mb_mbaff;               ///< mb_aff_frame && mb_field_decoding_flag

	int redundant_pic_count;

	/**
	 * number of neighbors (top and/or left) that used 8x8 dct
	 */
	int neighbor_transform_size;

	int direct_spatial_mv_pred;
	int col_parity;
	int col_fieldoff;

	int cbp;
	int top_cbp;
	int left_cbp;

	int dist_scale_factor[32];
	int dist_scale_factor_field[2][32];
	int map_col_to_list0[2][16 + 32];
	int map_col_to_list0_field[2][2][16 + 32];

	/**
	 * num_ref_idx_l0/1_active_minus1 + 1
	 */
	unsigned int ref_count[2];          ///< counts frames or fields, depending on current mb mode
	unsigned int list_count;
	H264Ref ref_list[2][48];        /**< 0..15: frame refs, 16..47: mbaff field refs.
										 *   Reordered version of default_ref_list
										 *   according to picture reordering in slice header */
	struct {
		uint8_t op;
		uint32_t val;
	} ref_modifications[2][32];
	int nb_ref_modifications[2];

	unsigned int pps_id;

	const uint8_t *intra_pcm_ptr;
	int16_t *dc_val_base;

	uint8_t *bipred_scratchpad;
	uint8_t *edge_emu_buffer;
	uint8_t(*top_borders[2])[(16 * 3) * 2];
	int bipred_scratchpad_allocated;
	int edge_emu_buffer_allocated;
	int top_borders_allocated[2];

	/**
	 * non zero coeff count cache.
	 * is 64 if not available.
	 */
	DECLARE_ALIGNED(8, uint8_t, non_zero_count_cache)[15 * 8];

	/**
	 * Motion vector cache.
	 */
	DECLARE_ALIGNED(16, int16_t, mv_cache)[2][5 * 8][2];
	DECLARE_ALIGNED(8, int8_t, ref_cache)[2][5 * 8];
	DECLARE_ALIGNED(16, uint8_t, mvd_cache)[2][5 * 8][2];
	uint8_t direct_cache[5 * 8];

	DECLARE_ALIGNED(8, uint16_t, sub_mb_type)[4];

	///< as a DCT coefficient is int32_t in high depth, we need to reserve twice the space.
	DECLARE_ALIGNED(16, int16_t, mb)[16 * 48 * 2];
	DECLARE_ALIGNED(16, int16_t, mb_luma_dc)[3][16 * 2];
	///< as mb is addressed by scantable[i] and scantable is uint8_t we can either
	///< check that i is not too large or ensure that there is some unused stuff after mb
	int16_t mb_padding[256 * 2];

	uint8_t(*mvd_table[2])[2];

	/**
	 * Cabac
	 */
	CABACContext cabac;
	uint8_t cabac_state[1024];
	int cabac_init_idc;

	MMCO mmco[MAX_MMCO_COUNT];
	int  nb_mmco;
	int explicit_ref_marking;

	int frame_num;
	int poc_lsb;
	int delta_poc_bottom;
	int delta_poc[2];
	int curr_pic_num;
	int max_pic_num;
} H264SliceContext;

typedef struct H264Context {
	const AVClass *class;
	AVCodecContext *avctx;
	VideoDSPContext vdsp;
	H264DSPContext h264dsp;
	H264ChromaContext h264chroma;
	H264QpelContext h264qpel;

	H264Picture DPB[H264_MAX_PICTURE_COUNT];
	H264Picture *cur_pic_ptr;
	H264Picture cur_pic;
	H264Picture last_pic_for_ec;

	H264SliceContext *slice_ctx;
	int            nb_slice_ctx;
	int            nb_slice_ctx_queued;

	H2645Packet pkt;

	int pixel_shift;    ///< 0 for 8-bit H.264, 1 for high-bit-depth H.264

	/* coded dimensions -- 16 * mb w/h */
	int width, height;
	int chroma_x_shift, chroma_y_shift;

	int droppable;
	int coded_picture_number;

	int context_initialized;
	int flags;
	int workaround_bugs;
	int x264_build;
	/* Set when slice threading is used and at least one slice uses deblocking
	 * mode 1 (i.e. across slice boundaries). Then we disable the loop filter
	 * during normal MB decoding and execute it serially at the end.
	 */
	int postpone_filter;

	/*
	 * Set to 1 when the current picture is IDR, 0 otherwise.
	 */
	int picture_idr;

	int crop_left;
	int crop_right;
	int crop_top;
	int crop_bottom;

	int8_t(*intra4x4_pred_mode);
	H264PredContext hpc;

	uint8_t(*non_zero_count)[48];

#define LIST_NOT_USED -1 // FIXME rename?
#define PART_NOT_AVAILABLE -2

	/**
	 * block_offset[ 0..23] for frame macroblocks
	 * block_offset[24..47] for field macroblocks
	 */
	int block_offset[2 * (16 * 3)];

	uint32_t *mb2b_xy;  // FIXME are these 4 a good idea?
	uint32_t *mb2br_xy;
	int b_stride;       // FIXME use s->b4_stride

	uint16_t *slice_table;      ///< slice_table_base + 2*mb_stride + 1

	// interlacing specific flags
	int mb_aff_frame;
	int picture_structure;
	int first_field;

	uint8_t *list_counts;               ///< Array of list_count per MB specifying the slice type

	/* 0x100 -> non null luma_dc, 0x80/0x40 -> non null chroma_dc (cb/cr), 0x?0 -> chroma_cbp(0, 1, 2), 0x0? luma_cbp */
	uint16_t *cbp_table;

	/* chroma_pred_mode for i4x4 or i16x16, else 0 */
	uint8_t *chroma_pred_mode_table;
	uint8_t(*mvd_table[2])[2];
	uint8_t *direct_table;

	uint8_t scan_padding[16];
	uint8_t zigzag_scan[16];
	uint8_t zigzag_scan8x8[64];
	uint8_t zigzag_scan8x8_cavlc[64];
	uint8_t field_scan[16];
	uint8_t field_scan8x8[64];
	uint8_t field_scan8x8_cavlc[64];
	uint8_t zigzag_scan_q0[16];
	uint8_t zigzag_scan8x8_q0[64];
	uint8_t zigzag_scan8x8_cavlc_q0[64];
	uint8_t field_scan_q0[16];
	uint8_t field_scan8x8_q0[64];
	uint8_t field_scan8x8_cavlc_q0[64];

	int mb_y;
	int mb_height, mb_width;
	int mb_stride;
	int mb_num;

	// =============================================================
	// Things below are not used in the MB or more inner code

	int nal_ref_idc;
	int nal_unit_type;

	int has_slice;          ///< slice NAL is found in the packet, set by decode_nal_units, its state does not need to be preserved outside h264_decode_frame()

	/**
	 * Used to parse AVC variant of H.264
	 */
	int is_avc;           ///< this flag is != 0 if codec is avc1
	int nal_length_size;  ///< Number of bytes used for nal length (1, 2 or 4)

	int bit_depth_luma;         ///< luma bit depth from sps to detect changes
	int chroma_format_idc;      ///< chroma format from sps to detect changes

	H264ParamSets ps;

	uint16_t *slice_table_base;

	H264POCContext poc;

	H264Ref default_ref[2];
	H264Picture *short_ref[32];
	H264Picture *long_ref[32];
	H264Picture *delayed_pic[MAX_DELAYED_PIC_COUNT + 2]; // FIXME size?
	int last_pocs[MAX_DELAYED_PIC_COUNT];
	H264Picture *next_output_pic;
	int next_outputed_poc;

	/**
	 * memory management control operations buffer.
	 */
	MMCO mmco[MAX_MMCO_COUNT];
	int  nb_mmco;
	int mmco_reset;
	int explicit_ref_marking;

	int long_ref_count;     ///< number of actual long term references
	int short_ref_count;    ///< number of actual short term references

	/**
	 * @name Members for slice based multithreading
	 * @{
	 */
	 /**
	  * current slice number, used to initialize slice_num of each thread/context
	  */
	int current_slice;

	/** @} */

	/**
	 * Complement sei_pic_struct
	 * SEI_PIC_STRUCT_TOP_BOTTOM and SEI_PIC_STRUCT_BOTTOM_TOP indicate interlaced frames.
	 * However, soft telecined frames may have these values.
	 * This is used in an attempt to flag soft telecine progressive.
	 */
	int prev_interlaced_frame;

	/**
	 * Are the SEI recovery points looking valid.
	 */
	int valid_recovery_point;

	/**
	 * recovery_frame is the frame_num at which the next frame should
	 * be fully constructed.
	 *
	 * Set to -1 when not expecting a recovery point.
	 */
	int recovery_frame;

	/**
	 * We have seen an IDR, so all the following frames in coded order are correctly
	 * decodable.
	 */
#define FRAME_RECOVERED_IDR  (1 << 0)
	 /**
	  * Sufficient number of frames have been decoded since a SEI recovery point,
	  * so all the following frames in presentation order are correct.
	  */
#define FRAME_RECOVERED_SEI  (1 << 1)

	int frame_recovered;    ///< Initial frame has been completely recovered

	int has_recovery_point;

	int missing_fields;

	/* for frame threading, this is set to 1
	 * after finish_setup() has been called, so we cannot modify
	 * some context properties (which are supposed to stay constant between
	 * slices) anymore */
	int setup_finished;

	int cur_chroma_format_idc;
	int cur_bit_depth_luma;
	int16_t slice_row[MAX_SLICES]; ///< to detect when MAX_SLICES is too low

	/* original AVCodecContext dimensions, used to handle container
	 * cropping */
	int width_from_caller;
	int height_from_caller;

	int enable_er;

	H264SEIContext sei;

	AVBufferPool *qscale_table_pool;
	AVBufferPool *mb_type_pool;
	AVBufferPool *motion_val_pool;
	AVBufferPool *ref_index_pool;
	int ref2frm[MAX_SLICES][2][64];     ///< reference to frame number lists, used in the loop filter, the first 2 are for -2,-1
} H264Context;


#endif
