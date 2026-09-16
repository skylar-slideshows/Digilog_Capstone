### $chct [N]^
STM32 -> Remote. Reports the number of physical channels present in the console to the remote program.
### $chnm [n] [name]^
STM32 -> Remote. Sets channel n's name in the GUI to reflect the console.
### $ud [n] [controlID]^
STM32 -> Remote. User has turned physical knob. Update it in GUI to new position.
### $sw [n] [switchID]^
STM32 -> Remote. User has toggled routing switch. Update it in GUI to new state.
### $sel [n]^
STM32 -> Remote. Indicate a channel selected in physical console. Sync to app.
### (No manual commanding) $ack [message]^
STM32 -> Remote. Indicate console's recept of any message (except ack) from remote.
###________________________________________________________________
### chnm [n] [name]
Remote -> STM32. User changed name in remote. Set it in console.
### ud [n] [controlID]
Remote -> STM32. User changed knob in remote. Update it in console.
### sw [n] [switchID]
Remote -> STM32. User toggled switch in remote. Update it in console.
### sel [n]
Remote -> STM32. Indicate a channel selected on remote app.
### sett [s] [value]
Remote -> STM32. Update a setting.
### md [t] [data]
Remote -> STM32. Send mass data (entire channel's values).
t = 0 -> channel info, t = 1 -> console settings, t = 2 -> insert/chain routing
md begin // md end
### ack [message]
Remote -> STM32. Indicate remotes's recept of any message (except ack) from console.
###________________________________________________________________
n = Channel
###________________________________________________________________
###controlID:
0 = GAIN (Input)
1 = AUX 1 (Send)
2 = PAN (Send 1)
3 = AUX 2 (Send)
4 = PAN (Send 2)
5 = AUX 3 (Send)
6 = PAN (Send 3)
7 = AUX 4 Mono (Send)
8 = HF Gain (EQ)
9 = HF Freq (EQ)
10 = HMF Gain (EQ)
11 = HMF Freq (EQ)
12 = HMF Q (EQ)
13 = LMF Gain (EQ)
14 = LMF Freq (EQ)
15 = LMF Q (EQ)
16 = LF Gain (EQ)
17 = LF Freq (EQ)
18 = Input (Comp)
19 = Attack (Comp)
20 = Threshold (Comp)
21 = Release (Comp)
22 = De-Ess (Comp)
23 = Ratio (Comp)
24 = Output (Comp)
25 = PAN (extra knob)
26 = Fader
###________________________________________________________________
###switchID:
0 = Input Source (cycle: 0 Line / 1 Mic / 2 Hi-Z)
1 = Phase (PHS)
2 = +48V
3 = HPF
4 = Solo
5 = Mute
6 = Insert (INS)
7 = Pre-fader insert
###________________________________________________________________
###s:
