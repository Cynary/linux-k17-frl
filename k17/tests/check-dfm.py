from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
src=root/'drivers/gpu/drm/i915/display'
s=(src/'intel_hdmi_frl_dfm.c').read_text()
s=s[s.index('/* DFM constraints'):s.index('/* Get required no. of tribytes (estimate1)')]
h=(src/'intel_hdmi_frl_dfm.h').read_text()
h=h[h.index('struct intel_hdmi_frl_dfm_input_config'):h.index('bool intel_hdmi_frl_dfm_nondsc')]
prefix='''#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
typedef uint32_t u32;
typedef uint64_t u64;
#define U32_MIN 0U
#define max(a,b) ((a)>(b)?(a):(b))
#define min(a,b) ((a)<(b)?(a):(b))
#define DIV_ROUND_UP(n,d) (((n)+(d)-1)/(d))
#define DIV_ROUND_UP_ULL(n,d) DIV_ROUND_UP(n,d)
#define div_u64(n,d) ((u64)(n)/(d))
#define mult_frac(x,n,d) ({typeof(x) x_=(x); typeof(n) n_=(n); typeof(d) d_=(d); typeof(x_) q=x_/d_; typeof(x_) r=x_%d_; q*n_+r*n_/d_;})
#define DRM_OUTPUT_COLOR_FORMAT_RGB444 0
#define DRM_OUTPUT_COLOR_FORMAT_YCBCR420 1
#define DRM_OUTPUT_COLOR_FORMAT_YCBCR422 2
#define DRM_OUTPUT_COLOR_FORMAT_YCBCR444 3
'''
main='''
int main(void) {
 for (unsigned bpc=8;bpc<=12;bpc+=2) {
  for (unsigned rate=24;rate<=48;rate+=8) {
   struct intel_hdmi_frl_dfm d={.config={.pixel_clock_nominal_khz=1188000,.hactive=3840,.hblank=560,.bpc=bpc,.color_format=0,.lanes=4,.bit_rate_kbps=rate*1000000/4,.audio_hz=192000,.audio_channels=8}};
   bool ok=intel_hdmi_frl_dfm_nondsc_requirement_met(&d);
   assert(d.params.ftb_avg_k == (u64)d.params.pixel_clock_max_khz*bpc/8);
   assert(ok == (rate >= bpc*4));
   printf("4k120 %ubpc FRL%u: accepted=%d ftb_avg=%u expected=%llu line=%u borrowed=%u audio=%u active_min=%u blank_min=%u\\n",bpc,rate,ok,d.params.ftb_avg_k,(unsigned long long)d.params.pixel_clock_max_khz*bpc/8,d.params.line_time_ns,d.params.tb_borrowed,d.params.num_audio_pkts_line,get_tactive_min(4,d.params.tb_active,d.params.overhead_max,d.params.char_rate_min_kbps),get_tblank_min(4,d.params.tb_blank,d.params.overhead_max,d.params.char_rate_min_kbps));
  }
 }
}
'''
with tempfile.TemporaryDirectory(prefix='k17-dfm-') as work:
    output=Path(work)/'dfm-test.c'
    binary=Path(work)/'dfm-test'
    output.write_text(prefix+h+s+main)
    subprocess.run(['gcc','-O2',str(output),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
