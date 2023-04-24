/*
 * struct definitions taken from HAMLIB repository,
 * file rigs/icom/icom.h
 */

#define ICOM_MAX_SPECTRUM_FREQ_RANGES 20

struct icom_agc_level
{                   
    enum agc_level_e           
    level; /*!< Hamlib AGC level from agc_level_e enum, the last entry should have level -1 */
    unsigned char
    icom_level; /*!< Icom AGC level for C_CTL_FUNC (0x16), S_FUNC_AGC (0x12) command */
};                             

struct icom_spectrum_edge_frequency_range
{   
    int range_id; /*!< ID of the range, as specified in the Icom CI-V manuals. First range ID is 1. */
    freq_t low_freq; /*!< The low edge frequency if the range in Hz */
    freq_t high_freq; /*!< The high edge frequency if the range in Hz */
};

struct icom_spectrum_scope_caps
{
    int spectrum_line_length; /*!< Number of bytes in a complete spectrum scope line */
    int single_frame_data_length; /*!< Number of bytes of specrtum data in a single CI-V frame when the data split to multiple frames */
    int data_level_min; /*!<  */
    int data_level_max;
    double signal_strength_min;
    double signal_strength_max;
};

struct icom_priv_caps
{
    unsigned char re_civ_addr;  /*!< The remote equipment's default CI-V address */
    int civ_731_mode; /*!< Off: freqs on 10 digits, On: freqs on 8 digits plus passband setting */
    // According to the CI-V+ manual the IC-781, IC-R9000, and IC-R7000 can select pas$
    // The other rigs listed apparently cannot and may need the civ_731_mode=1 which are 
    // 1-706
    // 2-706MKII
    // 3-706MKIIG
    // 4-707
    // 5-718
    // 6-746
    // 7-746PRO
    // 8-756
    // 9-756PRO
    // 10-756PROII
    // 11-820H
    // 12-821H
    // 13-910H
    // 14-R10
    // 15-R8500
    // 16-703
    // 17-7800

    int no_xchg; /*!< Off: use VFO XCHG to set other VFO, On: use set VFO to set other VFO */
    const struct ts_sc_list *ts_sc_list;
    // the 4 elements above are mandatory
    // everything below here is optional in the backends
    int settle_time; /*!< Receiver settle time, in ms */
    int (*r2i_mode)(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width,
                    unsigned char *md, signed char *pd); /*!< backend specific code
                               to convert bandwidth and 
                               mode to cmd tokens */
    void (*i2r_mode)(RIG *rig, unsigned char md, int pd, 
                     rmode_t *mode, pbwidth_t *width);    /*!< backend specific code
                               to convert response
                               tokens to bandwidth and 
                               mode */
    int antack_len;             /*!< Length of 0x12 cmd may be 3 or 4 bytes as of 2020-01-22 e.g. 7851 */
    int ant_count;              /*!< Number of antennas */
    int serial_full_duplex;     /*!< Whether RXD&TXD are not tied together */
    int offs_len;               /*!< Number of bytes in offset frequency field. 0 defaults to 3 */
    int serial_USB_echo_check;  /*!< Flag to test USB echo state */
    int agc_levels_present;     /*!< Flag to indicate that agc_levels array is populated */
    struct icom_agc_level agc_levels[HAMLIB_MAX_AGC_LEVELS + 1]; /*!< Icom rig-specific AGC levels, the last entry should have level -1 */
    struct icom_spectrum_scope_caps spectrum_scope_caps; /*!< Icom spectrum scope capabilities, if supported by the rig. Main/Sub scopes in Icom rigs have the same caps. */
    struct icom_spectrum_edge_frequency_range spectrum_edge_frequency_ranges[ICOM_MAX_SPECTRUM_FREQ_RANGES]; /*!< Icom spectrum scope edge frequencies, if supported by the rig. Last entry should have zeros in all fields. */
    struct cmdparams *extcmds;  /*!< Pointer to extended operations array */
    int dualwatch_split;        /*!< Rig supports dual watch for split ops -- e.g. ID-5100 */
};

