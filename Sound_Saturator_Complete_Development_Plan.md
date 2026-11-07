# Sound Saturator

## Complete development plan v1.0

| Item | Decision |
| --- | --- |
| Plan date | 7 November 2026 |
| Product | Sound Saturator |
| Delivery | Windows 11 x64 VST3 audio effect |
| Primary host | Akai MPC 3 Desktop, with the exact host build recorded during certification |
| Core purpose | An all-in-one saturation box for drums, breakbeats, bass, samples and restrained bus processing |
| Architecture | Colour and tube stages before a central VCA, followed by magnetic transient processing, tape saturation and output iron |
| Current status | Research-backed specification with numerical software experiments. Hardware calibration is an open development milestone |
| Starting material | Drum Forge v3.0 and the supplied summary, reworked around the magnetic stage and the added tube amplifier reference |

The signature sound should combine rounded peaks, dense body, useful bass harmonics and controllable playback weight. The VCA sets the envelope and feeds the magnetic section through an explicit makeup control. Each source of colour must remain independently audible and measurable.

## 1. Product priorities

1. Establish convincing magnetic transient behaviour and record/playback tape saturation on real drum loops.
2. Make the VCA-to-magnetic drive interaction predictable and musically useful.
3. Add a tube amplifier model with a documented hardware reference and a defined calibration programme.
4. Preserve the original transformer, germanium, Class-A and pre/post EQ capabilities.
5. Deliver reliable automation, project recall, bypass, parallel processing and matched-level comparison.
6. Build the final interface after the core audio passes its listening and measurement gates.

The release uses a fixed processing order. Module bypasses provide combinations ranging from a single saturation stage to the complete chain. Startup settings use unity trims, flat EQ, bypassed colour/tube/Flux/tape/output-iron stages, 1:1 VCA operation and 100% wet. The prepared delay remains active so subsequent processing changes retain consistent host timing.

Akai documents third-party VST3 scanning in MPC 3 Desktop. Certification must exercise Sound Saturator as an insert effect inside the actual MPC build used for release. [Akai scanning guidance](https://support.akaipro.com/en/support/solutions/articles/69000822863-akai-pro-mpc-2-software-scanning-3rd-party-vsts-inside-mpc2-and-mpc-beats)

## 2. Research findings and resulting decisions

| Reference | Verified finding | Sound Saturator decision |
| --- | --- | --- |
| Tegeler Magnetismus 2 | VCA compression and makeup feed a transient-processing stage, followed by an output transformer | Preserve this order within the centre and right-hand sections |
| Tegeler Flux modes | Iron affects the broadest transient window, Cobalt is intermediate and Nickel is narrowest | Treat Flux as a time-window control and measure its effective duration |
| TK Audio Transformer | Separate transformer, passive saturation and germanium circuits feed a blend, with a subsequent Class-A section | Retain a compact colour rack with distinct algorithms and trims |
| McIntosh MC275 Mk V | Documented valve amplifier with independent distortion, bandwidth and power measurements | Use one specific revision as the tube reference |
| Otari MX-5050BII-2 | Documented record/reproduce system and alignment procedures | Use a specified two-track tape configuration as the initial tape calibration target |
| Physical tape research | Record-head field, magnetic state and playback-head response can be modelled separately | Develop and validate each part of the tape path |

The original magnetic transient concept and the added tape model have separate controls and acceptance tests. Flux governs transient processing. Tape parameters govern the record/playback model. This gives the requested magnetic rounding, saturation and playback boost distinct engineering targets. [Tegeler product description](https://www.tegeler.com/en-US/Magnetismus2) · [Tegeler explanation](https://www.tegeler.com/en-US/News/Details/189) · [TK Audio stages](https://www.tkaudio.se/product/transformer/)

## 3. Complete signal flow

| Order | Stage | Function |
| --- | --- | --- |
| 1 | Input and dry tap | Capture the original input for parallel mix, bypass and comparison. Apply Input Trim to the processing path |
| 2 | Pre EQ | Shape the spectrum entering all nonlinear stages |
| 3 | Colour rack | Input transformer, passive saturation, germanium, colour blend and Class-A colour |
| 4 | Tube | MC275 Mk V-inspired input/driver, push-pull output and amplifier-transformer response |
| 5 | VCA | Linked compression with controlled timing and sidechain filtering |
| 6 | Makeup → Magnet | Manual gain after compression, directly controlling magnetic input level |
| 7 | Magnetic Flux | Time-windowed nonlinear transient bending |
| 8 | Tape | Record shaping, effective bias, stateful magnetic saturation and playback response |
| 9 | Tape Return and output iron | Clean tape-return gain followed by separately driven transformer colour |
| 10 | Post EQ | Final bass balance, presence, treble and filtering |
| 11 | Global Mix | Linear blend of the processed signal and latency-aligned original input |
| 12 | Final Output | Clean output trim, optional fixed comparison gain and output meters |

The VCA is the centre of the processing architecture and the front panel. Tube colour precedes it. Flux and tape follow it. The tube amplifier includes its own transformer behaviour, while the final output iron remains an independent creative stage.

Use a shared oversampled processing region for stages 3–9. Pre and Post EQ run at the host sample rate. Local colour blends run inside the shared region, with aligned branch timing.

Global bypass selects the original input delayed by the reported plugin latency. At Mix 0%, unity Final Output and comparison gain disabled, the output must equal that same delayed input.

A fully neutral chain selects a delay-only identity path. Transitions into and out of that state use the same click-safe bypass mechanism.

## 4. Calibration and gain controls

Define the internal reference using the peak amplitude of a calibration sine. The initial reference is a sine whose sample peak is −18 dBFS. Meters must distinguish this calibration from RMS dBFS, sample peak and dBTP.

| Control | Initial range | Audible consequence |
| --- | --- | --- |
| Input Trim | −24 to +24 dB | Changes drive through the entire wet path |
| Tube Drive | 0 to +30 dB | Moves the amplifier model towards and beyond its clipping region |
| Tube Trim | −30 to +12 dB | Sets the level sent from the tube stage into the VCA |
| VCA Makeup | −12 to +24 dB | Changes level entering Flux and tape |
| Magnetic Drive | 0 to +24 dB | Drives transient bending independently of VCA makeup |
| Tape Drive | 0 to +24 dB | Drives the record/media model |
| Tape Return | −12 to +6 dB | Adds clean return level before output iron |
| Output Iron Drive | 0 to +18 dB | Changes final transformer saturation |
| Final Output | −36 to +12 dB | Sets delivered level after global Mix |

These are engineering starting ranges. Final ranges follow overload, noise-floor, listening and automation tests. Any automatic drive compensation belongs to its named stage and its state must be visible.

The requested tape boost has two explicit components. Tape Body changes the playback low-frequency contour. Tape Return changes clean level. A boost into output iron can create further saturation. Final Output and fixed loudness matching allow comparisons at controlled listening levels.

## 5. Magnetic transient engine

### Controls and detector

Expose Flux, Intensity, Magnetic Drive and Attack Preserve on the main panel. Keep detector sensitivity and recovery in Advanced controls.

Derive a stereo-linked onset score from fast and slow envelopes. Tap the detector after VCA gain reduction and before makeup. This separates the envelope window from the makeup gain that drives the nonlinear audio path. Use a silence gate and bounded detector outputs. Each audio channel keeps its own processing state.

The initial implementation compares three candidates on the same stimuli.

| Candidate | Mechanism | Main evaluation |
| --- | --- | --- |
| A | Smooth onset mask controls the nonlinear residual added to the signal | Mask sidebands, peak rounding and body retention |
| B | Peak curvature and slew influence a bounded nonlinear bend | Sensitivity to sample rate, hats and sub-bass |
| C | Recent envelope and peak history control nonlinear curvature and recovery | Closely spaced hits, recovery and repeatability |

An implementable starting equation for A is

\[
u[n]=x[n]+I\,m_F[n]\left(\frac{f(gx[n])}{g}-x[n]\right).
\]

Here `I` is Intensity, `m_F` is the smoothed Flux mask, `g` is drive and `f` has unity small-signal slope. The equation provides a true neutral result at Intensity zero. Attack Preserve reduces the earliest part of the mask through a smooth envelope.

### Flux development targets

| Mode | Initial window search range | Intended result |
| --- | --- | --- |
| Nickel | 2–5 ms | Concentrated peak rounding |
| Cobalt | 6–14 ms | Broader attack transformation |
| Iron | 15–30 ms | Wider rounding and denser attack/body transition |

These ranges are prototype search bounds selected for development. Hardware captures determine any subsequent reference-matched settings. Measure effective width using the contiguous region above 10% of peak modification on isolated, level-controlled hits.

Select the winning candidate using transient envelopes, spectra, intermodulation and blind matched-level listening. Promote a candidate only after it handles fast retriggering, bass beneath snare hits and hard-panned percussion. Keep the detector and nonlinear algorithms separable so capture-derived refinements remain local.

## 6. Tape saturation and playback model

### Initial physical reference

Use an Otari MX-5050BII-2 with quarter-inch, two-track record/reproduce heads. Target 15 ips, NAB equalisation and 250 nWb/m reference fluxivity. Acquire fresh RTM SM911 for the capture session and align the actual machine to that stock. Record its tape batch, head condition and measured bias setting.

Otari alignment documentation supplies a 7½/15 ips, NAB, 250 nWb/m procedure originally specified with Ampex 456. RTM supplies SM911 performance curves under its own documented measurement conditions. Preserve both sets of conditions in the research record, and derive the combined Otari/SM911 profile from captures of that configuration. [Otari alignment procedure](https://otarimx5050.com/library/manuals/bii-mkiii-2-alignment/introduction/) · [SM911 manufacturer product](https://us.recordingthemasters.com/products/sm911) · [SM911 technical data](https://www.audiomarketing.net/RECTHEMASTERS/fiche_sm911_rtm_2022.pdf)

### Processing components

1. Model record electronics and record equalisation at small and high signal levels.
2. Convert audio drive to a normalised record-field variable.
3. Apply a bias-dependent magnetic model with bounded per-channel state.
4. Model playback loss and equalisation, including measured low-frequency head response.
5. Apply Tape Body scaling and Tape Return gain.

Tape Body scales a calibrated playback contour. Start with a bounded 0–3 dB body range around the measured bump frequency. Frequency and width come from the physical capture. Record and playback response are fitted together at low level, then checked across drive levels.

The Loop Print preset applies this record/playback model to incoming musical loops. Printing and reimporting audio produces successive saturation generations. Test one, two and four printed generations for accumulating DC, low-frequency buildup and high-frequency loss.

### Offline physical reference

Implement the published Jiles–Atherton-based tape model as an offline research reference, including its record and playback components. Handle its near-zero functions with stable series expansions and verify solver convergence over the full stimulus matrix. Maintain a physical-parameter record for each reference configuration. [Chowdhury, DAFx 2019](https://www.dafx.de/paper-archive/2019/DAFx2019_paper_3.pdf)

The Otari manual specifies a 133 kHz bias/erase oscillator. At a 48 kHz host rate, 4× processing has a 96 kHz Nyquist frequency, making a literal 133 kHz bias oscillator incompatible with that internal rate. The research implementation must resolve bias and its nonlinear products at a substantially higher rate. Start at 1.536 MHz and compare against 3.072 MHz, increasing the rate until the measured audio-band result converges. [Otari factory manual](https://www.worldradiohistory.com/Archive-Catalogs/Miscellaneous-Manufacturers/Otari-MX-5050BII-Manual-1984.pdf)

### Real-time implementation

Fit a reduced, bias-averaged state-space model to the converged physical reference and subsequently to hardware captures. Bias changes its fitted magnetic response and playback behaviour. The fit must cover nominal, lower and higher bias settings measured under a recorded calibration procedure.

A bounded play/relaxation bank is the first reduced-model candidate

\[
p_i[n]=\operatorname{clip}\!\left(\lambda_i p_i[n-1],h[n]-r_i,h[n]+r_i\right),
\]

\[
M[n]=a_0\tanh(h[n])+\sum_i a_i\tanh(p_i[n]),\qquad
\lambda_i=e^{-1/(\tau_i F_s)}.
\]

Use nonnegative weights with `a0 + sum(ai) = 1`, positive widths and time constants, and a declared small-signal calibration. The relaxation models effective record/playback memory. Its parameters are fitted engineering quantities. Validate other reduced state-space formulations if this candidate misses measured bias response, minor-loop behaviour or recovery.

The final tape algorithm must demonstrate the following behaviours separately.

- A decreasing incremental gain as record level rises, with corresponding harmonic growth.
- Different probe responses after controlled prior histories, with Flux disabled.
- Measured bias-dependent changes in distortion, headroom and frequency response.
- Playback contour changes that remain distinguishable from drive and clean return gain.
- Bounded decay to silence and stable behaviour on sustained low notes.

The shipping quality mode is selected after the reduced-model fit, aliasing and CPU gates pass. The offline reference remains the comparison instrument throughout development.

## 7. Tube reference and modelling programme

### Selected amplifier

Use the **McIntosh MC275 Mk V**, the revision documented by McIntosh for 2007–2012 and reviewed by Stereophile in 2010. It is a long-established high-end valve amplifier with public technical documentation and independent bench measurements. This makes it a practical reference for the requested premium tube stage. [Official revision page](https://www.mcintoshlabs.com/legacy-products/amplifiers/mc275-version-5) · [Independent review](https://www.stereophile.com/content/mcintosh-mc275-power-amplifier)

The Mk V manual specifies 75 W per channel, four KT88/6550 output valves, three 12AX7A input/phase-inverter valves and four 12AT7 driver valves. Its Unity Coupled output circuit uses a bifilar-wound transformer. [Mk V owner's manual](https://www.mcintoshlabs.com/-/media/Files/mcintoshlabs/DocumentMaster/us/Legacy/MC275v5-WV-OWNERS.pdf)

Independent measurements report approximately 85 W into a matching 8 Ω load at the review's 1% THD+N clipping criterion. They show low distortion through much of its range, second-harmonic prominence in the midrange, and stronger third/higher content under some low-frequency, higher-power conditions. These observations define the model's calibration programme. [Stereophile measurements](https://www.stereophile.com/content/mcintosh-mc275-power-amplifier-measurements)

### Reference configuration

Pin the first capture target to Mk V stereo operation, balanced input, the 8 Ω output tap and an 8 Ω resistive measurement load. Record the exact valves, channel, serial/revision information, warmup condition and capture levels. A suitably equipped amplifier measurement lab supplies the captures.

For reference scaling, 75 W into 8 Ω corresponds to approximately 24.49 V RMS by `V = sqrt(P R)`. Keep this analogue scale in capture metadata. Expose normalised Tube Drive and Tube Trim in the plugin.

### Production algorithm

Build a reduced circuit-informed model with these components.

| Component | Implementation target |
| --- | --- |
| Input/driver | Smooth asymmetric transfer and fitted frequency response |
| Phase splitter | Complementary branch drive with fitted balance |
| Push-pull output | Two smooth conduction branches combined differentially |
| Amplifier transformer | Fitted bandwidth and frequency-dependent nonlinear contribution |
| Feedback response | Closed-loop gain and distortion behaviour fitted across frequency and level |
| Dynamic states | Coupling/recovery terms supported by burst measurements |

Balanced push-pull branches cancel their even-order contributions. Fit any residual asymmetry to reference measurements. Match midband H2, bass H2/H3, the clipping knee, transient recovery and two-tone behaviour together. Add supply-recovery behaviour when the captures establish its magnitude and time scale.

At modest drive, target the amplifier's low-distortion operating region. Higher drive crosses its measured knee and then enters the documented artistic extension of the model.

Represent memoryless conduction with smooth analytical functions or monotone interpolated tables. Evaluate feedback through a fitted closed-loop model or a bounded solver with a documented residual limit. Drive and trim remain independent. Label high-drive artistic extrapolations in the model documentation.

The front panel names this section **Tube · KT88**. Research documentation identifies the reference amplifier and calibration configuration.

## 8. Numerical experiments completed for this plan

The following experiments exercise two starting algorithms. Their parameters are development seeds. Hardware calibration and full-band audio validation follow in the roadmap.

### Static tube transfer experiment

Evaluate an asymmetric driver followed by a smooth symmetric limiting function

\[
u=x+0.008\frac{x^2}{1+x^2},\qquad
y=\frac{u}{(1+|u|^{16})^{1/16}}.
\]

Use a 1 kHz sine at 768 kHz for 0.25 seconds. Measure the fundamental and harmonics 2–20 with a coherent FFT. Solve the input amplitude producing 1% THD, approximately `0.961142`, and define that amplitude as 0 dBr for this experiment.

| Relative input drive | THD | H2 | H3 |
| --- | --- | --- | --- |
| −20 dBr | 0.0381% | 0.0381% | Below numerical significance |
| −6 dBr | 0.1562% | 0.1560% | 0.000016% |
| 0 dBr | 1.0000% | 0.1414% | 0.8199% |
| +6 dBr | 21.8356% | 0.0701% | 21.3920% |

The experiment establishes a controllable transition from even-order colour to strong limiting distortion. Its single-frequency transfer is an audition prototype. The production model in section 7 requires the amplifier's frequency, dynamic and closed-loop measurements. The table reports model THD. The amplifier review uses a THD+N clipping criterion.

### Stateful magnetic experiment

Run the section 6 reduced-model equations at 192 kHz with `r = [0.04, 0.12, 0.25]`, `tau = [3, 12, 35] ms`, `a0 = 0.65` and `a = [0.10, 0.12, 0.13]`. Start each run from zero state. Condition for 192 samples at either `+0.85` or `−0.85`, then apply identical final inputs `[0.398, 0.399, 0.400]`.

| Prior condition | Final input | Final normalised magnetic output |
| --- | --- | --- |
| Positive conditioning | 0.400 | 0.414928 |
| Negative conditioning | 0.400 | 0.333592 |

The shared final input and slope produce different outputs because the candidate retains state. The physical calibration gate must establish the appropriate amount and duration of that memory. This small experiment covers state dependence. Tape bias, replay response and listening quality have their own tests.

## 9. VCA compressor

Implement a feedforward compressor with a stereo-linked detector, smooth knee, sidechain HPF and explicit post-compression makeup. Begin with peak and RMS-like detector candidates and select defaults using drum-loop listening and envelope measurements.

| Parameter | Initial range or behaviour |
| --- | --- |
| Threshold | −48 to 0 dBFS, using the documented detector level convention |
| Ratio | 1:1, 2:1, 4:1, 8:1 and limiter-like |
| Attack | 0.1–100 ms in Advanced mode |
| Release | 10–2000 ms in Advanced mode |
| Knee | 0–12 dB |
| Sidechain HPF | Off or 20–250 Hz |
| Stereo link | Common gain reduction, driven by the larger channel detector by default |
| Makeup | Manual, after gain reduction |

For a hard knee above threshold, use `output_dB = T + (input_dB − T) / R`. Define the knee continuously and test both its value and slope at the boundaries. A one-pole envelope coefficient is `exp(−1 / (tau Fs))`, with its time-constant convention stated in the implementation.

The main Timing selector uses the following authored, drum-oriented attack/release pairs.

| Position | Attack | Release |
| --- | --- | --- |
| 1 | 0.1 ms | 20 ms |
| 2 | 0.3 ms | 40 ms |
| 3 | 1 ms | 60 ms |
| 4 | 3 ms | 100 ms |
| 5 | 7 ms | 140 ms |
| 6 | 10 ms | 220 ms |
| 7 | 20 ms | 350 ms |
| 8 | 30 ms | 600 ms |
| 9 | 60 ms | 1000 ms |
| 10 | 100 ms | 2000 ms |

Retain these as authored settings in the model metadata. Source any hardware-matched timing table from verified manual values and captures. Tegeler publishes coupled timing endpoints and confirms that makeup changes transient-stage drive. [Tegeler timing and makeup](https://www.tegeler.com/en-US/Magnetismus2)

VCA bypass removes both gain reduction and its makeup. Ratio 1:1 retains manual makeup, providing a convenient drive-only workflow. At 1:1 and 0 dB makeup, this stage has unity gain.

## 10. Colour rack and mastering EQ

### Colour rack

Retain TX Weight, Passive Sat with an Even option, Germanium, Colour Blend and Class-A. Give each a measured sonic role, individual neutral/bypass behaviour and a clean trim where needed.

The TK reference describes low-frequency transformer harmonics, passive odd/even options, a predominantly even-harmonic germanium contribution and a subsequent Class-A stage. Its published low-bass contour supplies one reference point. Tune Sound Saturator's weight control against the material and the complete signal chain. [TK Audio technical description](https://www.tkaudio.se/product/transformer/)

Test low-frequency harmonics separately from bass EQ boost. Verify the different stages using matched-input spectra, static curves and two-tone signals. Apply controlled DC removal after asymmetric colour and tube sections and include its bass phase response in parallel-path tests.

### Pre and Post EQ

| Stage | Initial controls |
| --- | --- |
| Pre EQ | Low shelf 25–250 Hz, low-mid bell 100–1000 Hz, high-mid bell 1–8 kHz, air shelf 4–18 kHz |
| Post EQ | Bypassable HPF 12–80 Hz, broad bass shelf, presence bell, high shelf and LPF 4–20 kHz |
| Gain ranges | ±12 dB, with a fine-control option for small adjustments |
| Bell Q and shelf slope | Bell Q 0.35–2.5, starting at 0.707. Shelf slope S starts at 1 |
| Cut slopes | Selectable 12 or 24 dB/octave |
| Filter type | Minimum-phase biquads, with bounded frequency/Q and sample-rate-aware limits |

Implement the RBJ filter equations with double-precision coefficient design. Check poles at supported extremes and during control movement. Smooth physical controls and recompute stable filters, or crossfade prepared filter instances where required. Derive the displayed response from the actual filter coefficients. [W3C Audio EQ Cookbook](https://www.w3.org/TR/audio-eq-cookbook/)

Minimum-phase filtering rotates phase. Global delay alignment handles transport and oversampling delay. Measure frequency-dependent wet/dry interactions separately, especially with bass boosts and HPFs. Author Parallel Punch presets using those measured responses.

## 11. Oversampling, latency and real-time architecture

Start by benchmarking shared 2×, 4× and 8× nonlinear paths. Use linear-phase oversampling filters for a directly measurable constant delay. Choose the initial default from alias suppression, transient response and CPU results. Allocate buffers and prepare quality paths before audio starts.

For each host sample rate, establish a fixed integer plugin delay `D` covering the prepared quality paths. Pad each path to `D`. Use the oversampler's reported latency and integer-latency option, then verify the result with impulses. Report the same `D` to the host with `setLatencySamples`. Smooth quality changes through preallocated, aligned paths. [JUCE oversampling](https://docs.juce.com/develop/classjuce_1_1dsp_1_1Oversampling.html) · [JUCE processor latency](https://docs.juce.com/master/classjuce_1_1AudioProcessor.html)

Quality is an instance setting recalled with the project. It uses the same setting for playback and export. Test changes while the transport is running and account for the states of both paths during a transition.

| Engineering requirement | Implementation |
| --- | --- |
| Callback execution | Preallocated buffers, bounded work, CPU-based DSP and denormal control |
| Parameters | Stable IDs, block-safe command delivery and smoothing at each stage's processing rate |
| Stereo state | Independent channel states with explicit detector linking |
| Bypass | Click-safe selection of an aligned signal, with defined state warming or reset |
| Block handling | Zero-length calls, changing block sizes and chunking of blocks larger than the prepared capacity |
| Numeric robustness | Finite-value guards, bounded states and explicit recovery for invalid input |
| Silence | Predictable state decay and settling |
| UI and analysis | Audio-to-UI meter snapshots and background analysis |
| Presets | Versioned schema, stable defaults, migrations and checked bounds |
| Rendering | Reproducible results from the same initial state, quality and automation sequence |

Run offline physical modelling and parameter fitting in separate research tools. Ship the VST3 DSP in C++20 with JUCE and CMake. Use Python/NumPy/SciPy or C# for measurement analysis according to the developer's workflow.

## 12. Interface and metering

Use a resizable hardware-style panel with three main groups.

| Position | Main controls |
| --- | --- |
| Left | Colour selection, Colour Drive/Blend, Tube Drive and Tube Trim |
| Centre | VCA Threshold, Ratio, Timing, Makeup → Magnet and gain-reduction meter |
| Right, largest group | Flux, Intensity, Magnetic Drive, Tape Drive, Tape Body and Tape Return |
| Upper strip | Sound Saturator name, presets, A/B, Undo/Redo, bypass and Input Trim |
| Lower strip | Pre/Post EQ access, Output Iron, Mix, Final Output and meters |
| Advanced drawer | Bias profile, Attack Preserve, detector settings, EQ details, oversampling and diagnostic timing |

Meter the VCA gain reduction, magnetic input working level, transient activity, tape level/compression, output sample peak and true peak. Show units and numeric values for every control. Use current measured curves for EQ displays and clearly named scales for activity meters.

Compare the same passage using a fixed gain estimated from a sufficient repeated loop. Use integrated loudness for suitable passages and RMS/energy for isolated hits. Freeze comparison gain during playback. Re-estimate after a deliberate change to the comparison settings. [ITU-R BS.1770-5](https://www.itu.int/rec/R-REC-BS.1770-5-202311-I/en)

## 13. Hardware capture and calibration programme

### Capture targets

- Obtain repeatable Magnetismus 2 captures across Flux, Intensity, makeup, compression and output levels. Access Analog advertises a remotely operated physical unit, providing a potential capture route subject to session availability and measurement suitability. [Hardware access](https://accessanalog.com/product/tegeler-magnetismus-2/)
- Obtain the specified Otari/SM911 record/reproduce captures at nominal and deliberately offset bias settings, with calibration recorded.
- Obtain MC275 Mk V measurements from the defined amplifier/load configuration, including low-level transfer, level sweeps, bursts and driven conditions supported by the lab.
- Obtain colour-rack reference captures where available and preserve the published TK curves as the initial research baseline.

### Common procedure

1. Record device identity, revision, settings, source/return calibration and the interface path.
2. Capture an interface loopback and device bypass/reference path alongside processed output.
3. Use stepped sines at 50 Hz, 100 Hz, 1 kHz, 5 kHz and 10 kHz, with level sweeps across the useful operating range.
4. Add logarithmic sweeps, quiet/loud tone bursts, two-tone and multitone tests, alternating hit levels and real loops.
5. Repeat selected captures to quantify variability. Estimate delay and drift independently of tonal/phase differences.
6. Preserve original WAVs and metadata. Produce analysis copies with declared alignment and gain adjustments.
7. Divide fitting and evaluation material before tuning. Reserve whole loops and operating settings for held-out evaluation.

Capture metadata includes device/revision, tape stock/batch where relevant, load/tap where relevant, sample rate, source level convention, analogue reference, settings, routing, timestamp and checksums. Record any sample-rate conversion or streaming path.

Measure fundamental transfer, H2–H7, THD and THD+N under declared bandwidths, intermodulation, crest factor, true peak, onset/recovery, stereo gain/phase and noise. Interpret internal stage contributions using controllable settings and document uncertainty when the hardware exposes only final outputs.

## 14. Musical test library and presets

Build a rights-cleared corpus spanning dry acoustic and electronic kicks, 808/sub notes, bright/dark snares, rimshots, closed/open hats, shakers, mono breaks, stereo drum buses, melodic loops, sustained bass and vocal phrases. Include overlapping transients, reverb tails and hard-panned percussion.

Use 70, 90, 110 and 130 BPM patterns with the same underlying sounds. Test isolated hits and 2–8 bar passages. Keep development and held-out assets separate.

| Factory preset | Main purpose |
| --- | --- |
| Boom Bap Glue | Moderate VCA control with broad magnetic rounding |
| Tape Loop Print | Stateful tape saturation, playback body and controlled return gain |
| Heavy Kick | Low-frequency harmonics with bounded sub-bass energy |
| Snare Weight | Attack rounding with retained presence |
| Soft Hats | Narrow Flux window and restrained high-frequency drive |
| KT88 Silk | Mild tube colour with little magnetic processing |
| KT88 Push | Deliberate amplifier overdrive |
| Transformer Weight | Colour-rack focus with independently adjusted bass contour |
| Germanium Dust | Asymmetric colour and a softened finish |
| MPC Resample | Strong combined saturation with a controlled output level |
| Parallel Punch | Intense wet processing blended using verified path timing |
| Sample Warmer | Tube/tape colour on melodic material |
| Bass Translator | Upper harmonics that support bass audibility |
| Bus Gentle | Restrained settings across a broad input mix |

Evaluate every preset on several assets. Record perceived punch, density, harshness, low-end definition and groove at matched listening levels. Use at least three listeners for milestone reviews when available and record their individual responses.

## 15. Verification and acceptance gates

All tolerances below are proposed engineering targets. The gate report records the measured result, fixture and any agreed revision to a target.

| Area | Gate |
| --- | --- |
| Signature magnetic effect | Saturation presets produce a median crest-factor reduction of at least 1 dB across the designated drum-loop set, with useful punch and density confirmed in blind matched-level reviews |
| Flux distinction | Ordered effective widths on the calibrated hit set, with repeatable audible differences across the three modes |
| Tape state | Different outputs after controlled histories, plus fitted magnitude/recovery against the physical reference and held-out captures |
| Tape bias and playback | Capture-derived changes in harmonic response, headroom and replay contour reproduced across the specified bias profiles |
| Tube model | Low-level and driven behaviour fitted across frequency, level and bursts. Hardware plots and the model report retain their individual measurement conditions |
| Hardware small-signal fit | Aim for ±0.5 dB over 40 Hz–16 kHz after calibration, subject to capture uncertainty |
| Hardware nonlinear fit | Aim for ±3 dB on harmonics above −70 dBc and a clipping-knee location within ±1 dB on held-out settings |
| EQ | Magnitude error within 0.05 dB against the mathematical reference, with stable poles throughout automation |
| Aliasing | Against a converged high-rate reference, aim for aliased components below −80 dBFS at moderate drive and −65 dBFS at designated extreme settings |
| Aliasing protocol | Coherent 1, 5 and 9 kHz tones and multitone signals at 44.1/48 kHz. Separate intended harmonics/IMD from foldback and report test level, bandwidth and spectral method |
| Timing | Impulse-measured fixed delay equals host-reported `D`. Neutral processing and global bypass agree after delay alignment |
| Latency budget | Initial target of at most 128 host samples at 44.1/48 kHz for the selected real-time default |
| Parallel mix | Mix 0% matches delayed dry at unity output. Neutral 50% mix introduces no additional comb response. Character settings have documented partial-mix response |
| Silence/DC | With noise-free processing, residual output settles below −110 dBFS within two seconds after the defined moderate test burst |
| Numerical stress | Finite, bounded output/state on silence, denormals, declared high-level stress vectors and invalid-input recovery tests |
| Reproducibility | Same input, initial state, automation and quality produce equivalent renders across block partitions within a declared floating-point tolerance |
| CPU | Profile the documented Windows reference PC at 48 kHz/128 samples with 1, 8 and 16 instances. Eight default-quality instances target less than half of the audio callback budget |

Determine aliasing convergence with the same baseband model at higher rates. Validate the literal-bias physical reference separately. Any higher-quality setting must pass its own performance and delay tests.

### Host certification matrix

- Windows 11 x64, MPC 3 Desktop and one independent VST3 host such as REAPER.
- Mono and stereo at 44.1, 48, 88.2 and 96 kHz.
- Buffer lengths 32–2048, irregular partitions, zero-sized calls and oversized-block handling.
- Scan/load, multiple instances, editor open/close, resize/HiDPI and clean install.
- Parameter automation, presets, A/B, bypass, quality changes and transport cycling.
- Project save/reopen, schema migration, corrupted-state recovery and offline rendering.
- pluginval at strictness level 5 and the Steinberg VST3 validator, followed by targeted higher-stress runs. [pluginval](https://github.com/Tracktion/pluginval) · [VST3 SDK](https://github.com/steinbergmedia/vst3sdk)

## 16. Software work packages

| Component | Responsibility |
| --- | --- |
| Plugin processor | VST3 buses, lifecycle, callback scheduling, latency and bypass |
| Parameter registry | Stable identifiers, ranges, units, smoothing and defaults |
| Colour rack | TX, passive, germanium, blend and Class-A |
| Tube stage | Driver, differential output, transformer/feedback response and state |
| VCA stage | Detector, gain computer, knee, timing and makeup |
| Magnetic stage | Flux detector, nonlinear candidates, tape state and playback |
| Output stage | Output iron, Post EQ, Mix, clean output and metering |
| State/presets | Versioned serialisation, migrations, A/B and factory presets |
| Measurement tools | Stimulus generation, capture manifests, fitting and comparison reports |
| Regression suite | Mathematical, musical, state, performance and host tests |

Pin JUCE, VST3 SDK and tool versions in the build. Select a JUCE distribution licence before release. Steinberg documents MIT licensing from VST3 SDK 3.8. Preserve source notices and third-party dependency records. Research papers and public implementations supply references whose code licences must be checked before reuse. [JUCE licence](https://juce.com/legal/juce-8-licence/) · [VST3 licensing](https://steinbergmedia.github.io/vst3_dev_portal/pages/VST%2B3%2BLicensing/Index.html)

## 17. Development sequence and estimates

The estimates assume one experienced developer with access to specialist DSP review. They express engineering effort. Hardware access, learning and iteration affect the calendar. The combined starting estimate is approximately 24–49+ engineer-weeks.

| Milestone | Work | Approximate effort | Exit condition |
| --- | --- | --- | --- |
| M0 | Final parameters, fixtures, capture arrangements and musical corpus | 1–2 weeks | Repeatable reference set and test definitions |
| M1 | JUCE/CMake VST3 shell, state, gain, bypass and host smoke test | 1–2 weeks | Reliable pass-through insert in MPC 3 Desktop |
| M2 | VCA and three Flux candidates in a minimal interface | 3–6 weeks | Convincing VCA/magnetic interaction on held-out drums |
| M3 | Offline physical tape reference and reduced real-time model | 6–12+ weeks | Stateful tape, bias and playback gates met |
| M4 | MC275 Mk V-inspired tube model and capture fitting | 4–8 weeks | Distinct mild/driven tube behaviour and model report |
| M5 | Colour rack, Pre/Post EQ and integrated oversampling | 3–6 weeks | Complete signal path with measured latency and aliasing |
| M6 | Final interface, metering, presets and usability | 3–6 weeks | Every control connected to tested DSP and recallable state |
| M7 | Full regression, optimisation, host certification and packaging | 3–7 weeks | Release gates completed and calibration status documented |

Begin hardware capture during M0–M2 and iterate throughout the DSP milestones. Use basic gain controls and a functional interface to audition the magnetic core early.

### First integrated prototype

Implement the shell, VCA, explicit makeup, selected Flux candidate, provisional tape state, delayed Mix and Final Output. Evaluate it on the held-out drum corpus before completing decorative UI work. Then integrate the tube and colour stages into their defined positions.

The prototype review must establish three independent changes when controls move. Intensity changes transient shape. Tape Drive changes the sustained saturation behaviour. Tape Body changes playback weight. Reassess the algorithms if matched-level listening cannot distinguish their intended functions.

## 18. Release deliverables and completion condition

Deliver a versioned Windows x64 `Sound Saturator.vst3` bundle, installer, factory presets and a concise manual. Include the verified host matrix, latency/quality table, calibration report, dependency notices and regression summary in the release package.

The manual explains the central VCA, the makeup-to-magnetic relationship, Flux widths, tape bias/playback controls, tube reference, parallel behaviour and fixed comparison gain. Publish the actual calibration status of each stage.

The release is complete when the magnetic section delivers useful saturation and transient rounding across the held-out material, the tube stage meets its declared modelling targets, and project recall, automation, latency and real-time performance pass the certification matrix.

## 19. Decisions and open validation items

| Decision made for this plan | Validation still required |
| --- | --- |
| Sound Saturator replaces the Drum Forge product name | Product naming and artwork approval |
| Windows x64 VST3 is the delivery target | Actual MPC 3 Desktop and second-host certification |
| Tube and colour precede the central VCA | Full-chain listening and gain-staging review |
| Flux precedes a separate record/playback tape section | Candidate selection and hardware fitting |
| MC275 Mk V is the tube reference | Configuration-specific captures and dynamic model fitting |
| Otari BII-2/SM911 at 15 ips is the initial tape target | Captures of that exact machine, stock and alignment |
| Numerical experiments establish initial transfer/state behaviour | Full-band, real-time and musical verification |

### Source material

The factual hardware and platform references are linked beside their relevant decisions. The starting specification is available as a separate source document. [Drum Forge v3.0](https://chatgpt.com/library?file_id=libfile_9581bf2de4d881919f06f6d593b67eeb&account_id=personal)
