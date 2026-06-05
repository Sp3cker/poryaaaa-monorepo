	.align 2
voicegroup001::
	voice_directsound 60, 0, DirectSoundWaveData_PianoC4, 255, 165, 245, 165
	voice_square_1 60, 0, 0, 0, 165, 245, 165, 0
	voice_square_2 60, 0, 0, 165, 245, 165, 0
	voice_noise 60, 0, 0, 165, 245, 165, 0
	voice_programmable_wave 60, 0, ProgrammableWaveData_Sine, 165, 245, 165, 0
	voice_keysplit KeySplitTable_Drumkit, voicegroup_drumkit
	voice_keysplit_all voicegroup002
