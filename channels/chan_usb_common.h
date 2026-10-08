/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2007 - 2008, Jim Dixon
 *
 * Helpers shared by chan_simpleusb and chan_usbradio.
 *
 * Include once, after the private struct, the default instance, and the
 * parallel-port globals. The includer typedefs that struct as chan_usb_pvt
 * and defines the CHAN_USB_* macros. Functions that already share a name are
 * used as-is. The others take their name from the matching macro, so each
 * driver keeps its existing call sites. The functions are static: each module
 * still compiles its own copy.
 */

#ifndef CHAN_USB_COMMON_H
#define CHAN_USB_COMMON_H

#if !defined(CHAN_USB_DEFAULT) || !defined(CHAN_USB_ACTIVE) || !defined(CHAN_USB_DUDEUSB_GPIO_CTL) || \
	!defined(CHAN_USB_LOG_FAULT) || !defined(CHAN_USB_DEVICE_IDENTITY) || !defined(CHAN_USB_SWAP_BEGIN) || \
	!defined(CHAN_USB_SWAP_AUDIO_STOPPED) || !defined(CHAN_USB_SWAP_READY) || !defined(CHAN_USB_SWAP_FINISH) || \
	!defined(CHAN_USB_DIGIT_BEGIN) || !defined(CHAN_USB_ANSWER) || !defined(CHAN_USB_FIXUP) || !defined(CHAN_USB_SETOPTION)
#error chan_usb_common.h included without its driver macros
#endif

static chan_usb_pvt *store_config(struct ast_config *cfg, const char *ctg);

/*!
 * \brief Configure our private structure based on the
 * found hardware type.
 * \param o		Channel private data.
 * \returns 0	Always returns zero.
 */
static int hidhdwconfig(chan_usb_pvt *o)
{
	int i;

	/* NOTE: on the CM-108AH, GPIO2 is *not* a REAL GPIO.. it was re-purposed
	 *  as a signal called "HOOK" which can only be read from the HID.
	 *  Apparently, in a REAL CM-108, GPIO really works as a GPIO
	 */

	if (o->hdwtype == 1) {
		/* sphusb */
		o->hid_gpio_ctl = 0x08;	 /* set GPIO4 to output mode */
		o->hid_gpio_ctl_loc = 2; /* For CTL of GPIO */
		o->hid_io_cor = 4;		 /* GPIO3 is COR */
		o->hid_io_cor_loc = 1;	 /* GPIO3 is COR */
		o->hid_io_ctcss = 2;	 /* GPIO 2 is External CTCSS */
		o->hid_io_ctcss_loc = 1; /* is GPIO 2 */
		o->hid_io_ptt = 8;		 /* GPIO 4 is PTT */
		o->hid_gpio_loc = 1;	 /* For ALL GPIO */
		o->valid_gpios = 1;		 /* for GPIO 1 */
	} else if (o->hdwtype == 0) {
		/* dudeusb */
		o->hid_gpio_ctl = CHAN_USB_DUDEUSB_GPIO_CTL; /* dudeusb GPIO output mode */
		o->hid_gpio_ctl_loc = 2;					 /* For CTL of GPIO */
		o->hid_io_cor = 2;							 /* VOLD DN is COR */
		o->hid_io_cor_loc = 0;						 /* VOL DN COR */
		o->hid_io_ctcss = 1;						 /* VOL UP External CTCSS */
		o->hid_io_ctcss_loc = 0;					 /* VOL UP External CTCSS */
		o->hid_io_ptt = 4;							 /* GPIO 3 is PTT */
		o->hid_gpio_loc = 1;						 /* For ALL GPIO */
		o->valid_gpios = 0xfb;						 /* for GPIO 1,2,4,5,6,7,8 (5,6,7,8 for CM-119 only) */
	} else if (o->hdwtype == 2) {
		/* NHRC (N1KDO) (dudeusb w/o user GPIO) */
		o->hid_gpio_ctl = 0x04;	 /* set GPIO 3 to output mode */
		o->hid_gpio_ctl_loc = 2; /* For CTL of GPIO */
		o->hid_io_cor = 2;		 /* VOLD DN is COR */
		o->hid_io_cor_loc = 0;	 /* VOL DN COR */
		o->hid_io_ctcss = 1;	 /* VOL UP is External CTCSS */
		o->hid_io_ctcss_loc = 0; /* VOL UP CTCSS */
		o->hid_io_ptt = 4;		 /* GPIO 3 is PTT */
		o->hid_gpio_loc = 1;	 /* For ALL GPIO */
		o->valid_gpios = 0;		 /* for GPIO 1,2,4 */
	} else if (o->hdwtype == 3) {
		/* custom version */
		o->hid_gpio_ctl = 0x0c;	 /* set GPIO 3 & 4 to output mode */
		o->hid_gpio_ctl_loc = 2; /* For CTL of GPIO */
		o->hid_io_cor = 2;		 /* VOLD DN is COR */
		o->hid_io_cor_loc = 0;	 /* VOL DN COR */
		o->hid_io_ctcss = 2;	 /* GPIO 2 is External CTCSS */
		o->hid_io_ctcss_loc = 1; /* is GPIO 2 */
		o->hid_io_ptt = 4;		 /* GPIO 3 is PTT */
		o->hid_gpio_loc = 1;	 /* For ALL GPIO */
		o->valid_gpios = 1;		 /* for GPIO 1 */
	}
	/* validate clipledgpio setting (Clip LED GPIO#) */
	if (o->clipledgpio) {
		if (o->clipledgpio >= GPIO_PINCOUNT || !(o->valid_gpios & (1 << (o->clipledgpio - 1)))) {
			ast_log(LOG_ERROR, "Channel %s: clipledgpio = GPIO%d not supported\n", o->name, o->clipledgpio);
			o->clipledgpio = 0;
		} else {
			o->hid_gpio_ctl |= 1 << (o->clipledgpio - 1); /* confirm Clip LED GPIO set to output mode */
		}
	}
	o->hid_gpio_val = 0;
	for (i = 0; i < GPIO_PINCOUNT; i++) {
		/* skip if this one not specified */
		if (!o->gpios[i]) {
			continue;
		}
		/* skip if not out */
		if (strncasecmp(o->gpios[i], "out", 3)) {
			continue;
		}
		/* skip if PTT */
		if ((1 << i) & o->hid_io_ptt) {
			ast_log(LOG_ERROR, "Channel %s: You can't specify gpio%d, since its the PTT.\n", o->name, i + 1);
			continue;
		}
		/* skip if not a valid GPIO */
		if (!(o->valid_gpios & (1 << i))) {
			ast_log(LOG_ERROR, "Channel %s: You can't specify gpio%d, it is not valid in this configuration.\n", o->name, i + 1);
			continue;
		}
		o->hid_gpio_ctl |= (1 << i); /* set this one to output, also */
		/* if default value is 1, set it */
		if (!strcasecmp(o->gpios[i], "out1")) {
			o->hid_gpio_val |= (1 << i);
		}
	}
	if (o->invertptt) {
		o->hid_gpio_val |= o->hid_io_ptt;
	}
	return 0;
}

/*!
 * \brief Indicate that PTT is activate.
 *	This causes the hidthead to to exit from the loop timer and
 *	evaluate the gpio pins.
 * \param o		Channel private data.
 */
static void kickptt(const chan_usb_pvt *o)
{
	char c = 0;
	int res;

	if (!o) {
		return;
	}
	if (o->pttkick[1] == -1) {
		return;
	}
	res = write(o->pttkick[1], &c, 1);
	if (res < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		ast_log(LOG_ERROR, "Channel %s: Write failed: %s\n", o->name, strerror(errno));
	} else if (res == 0) {
		ast_log(LOG_ERROR, "Channel %s: Write returned 0 bytes unexpectedly\n", o->name);
	}
}

/*!
 * \brief Search our configured channels to find the
 *	one with the matching USB descriptor.
 *	Print a message if the descriptor was not found.
 * \param o		chan_usb_pvt
 * \returns		Private structure that matches or NULL if not found.
 */
static chan_usb_pvt *find_desc(const char *dev)
{
	chan_usb_pvt *o = NULL;

	for (o = CHAN_USB_DEFAULT.next; o && o->name && dev && strcmp(o->name, dev) != 0; o = o->next)
		;
	if (!o) {
		ast_log(LOG_WARNING, "Cannot find USB descriptor <%s>.\n", dev ? dev : "-- Null Descriptor --");
		return NULL;
	}

	return o;
}

/*!
 * \brief Parallel port processing thread.
 *	This thread evaluates the timers configured for each
 *  configured parallel port pin.
 * \param arg	Arguments - this is always NULL.
 */
static void *pulserthread(void *arg)
{
	struct timeval now, then;
	int i, j, k;

#ifdef HAVE_SYS_IO
	if (haspp == 2) {
		ioperm(pbase, 2, 1);
	}
#endif
	stoppulser = 0;
	pp_lastmask = 0;
	ast_mutex_lock(&pp_lock);
	ast_radio_ppwrite(haspp, ppfd, pbase, pport, pp_val);
	ast_mutex_unlock(&pp_lock);
	then = ast_radio_tvnow();

	while (!stoppulser) {
		usleep(50000);
		ast_mutex_lock(&pp_lock);
		now = ast_radio_tvnow();
		j = ast_tvdiff_ms(now, then);
		then = now;
		/* make output inversion mask (for pulseage) */
		pp_lastmask = pp_pulsemask;
		pp_pulsemask = 0;
		for (i = 2; i <= 9; i++) {
			k = pp_pulsetimer[i];
			if (k) {
				k -= j;
				if (k < 0) {
					k = 0;
				}
				pp_pulsetimer[i] = k;
			}
			if (k) {
				pp_pulsemask |= 1 << (i - 2);
			}
		}
		if (pp_pulsemask != pp_lastmask) { /* if anything inverted (temporarily) */
			pp_val ^= pp_lastmask ^ pp_pulsemask;
			ast_radio_ppwrite(haspp, ppfd, pbase, pport, pp_val);
		}
		ast_mutex_unlock(&pp_lock);
	}
	return NULL;
}

/*!
 * \brief Log a USB/audio fault and set the recovery latch.
 *
 * First occurrence (already_logged == 0) uses LOG_ERROR; repeats use DEBUG
 * so retry loops do not spam. Returns 1 for storing into a rate-limit latch.
 */
static int __attribute__((format(printf, 3, 4))) CHAN_USB_LOG_FAULT(chan_usb_pvt *o, int already_logged, const char *fmt, ...)
{
	va_list ap;
	char buf[512];

	o->usb_faulted = 1;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (already_logged) {
		ast_debug(1, "%s", buf);
	} else {
		ast_log(LOG_ERROR, "%s", buf);
	}
	return 1;
}

static void CHAN_USB_DEVICE_IDENTITY(chan_usb_pvt *o, char *devstr, size_t devstr_size, char *serial, size_t serial_size, int *alsa_card)
{
	if (devstr && devstr_size) {
		devstr[0] = '\0';
	}
	if (serial && serial_size) {
		serial[0] = '\0';
	}
	if (alsa_card) {
		*alsa_card = -1;
	}

	/* Report the acquired identity when the channel currently holds a lease */
	ast_mutex_lock(&o->device_lock);
	if (o->radio_device) {
		if (devstr && devstr_size) {
			ast_copy_string(devstr, o->radio_device->devstr, devstr_size);
		}
		if (serial && serial_size && o->radio_device->serial) {
			ast_copy_string(serial, o->radio_device->serial, serial_size);
		}
		if (alsa_card) {
			*alsa_card = o->radio_device->alsa_card;
		}
	}
	ast_mutex_unlock(&o->device_lock);
}

static void CHAN_USB_SWAP_BEGIN(chan_usb_pvt *o)
{
	ast_mutex_lock(&o->swap_lock);
	o->swap_audio_ready = 0;
	o->swap_state = DEVICE_SWAP_QUIESCING;
	ast_mutex_unlock(&o->swap_lock);
}

static void CHAN_USB_SWAP_AUDIO_STOPPED(chan_usb_pvt *o)
{
	ast_mutex_lock(&o->swap_lock);
	if (o->swap_state == DEVICE_SWAP_QUIESCING) {
		o->swap_audio_ready = 1;
	}
	ast_mutex_unlock(&o->swap_lock);
}

static int CHAN_USB_SWAP_READY(chan_usb_pvt *o)
{
	int ready;

	ast_mutex_lock(&o->swap_lock);
	ready = o->swap_state == DEVICE_SWAP_READY;
	ast_mutex_unlock(&o->swap_lock);
	return ready;
}

static void CHAN_USB_SWAP_FINISH(chan_usb_pvt *o)
{
	ast_mutex_lock(&o->swap_lock);
	o->swap_audio_ready = 0;
	o->swap_state = DEVICE_SWAP_IDLE;
	ast_mutex_unlock(&o->swap_lock);
}

/*!
 * \brief Asterisk digit begin function.
 * \param c				Asterisk channel.
 * \param digit			Digit processed.
 * \retval 0
 */
static int CHAN_USB_DIGIT_BEGIN(struct ast_channel *c, char digit)
{
	return 0;
}

/*!
 * \brief Answer the call.
 * \param c				Asterisk channel.
 * \retval 0 			If successful.
 */
static int CHAN_USB_ANSWER(struct ast_channel *c)
{
	ast_setstate(c, AST_STATE_UP);
	return 0;
}

/*!
 * \brief Asterisk fixup function.
 * \param oldchan		Old asterisk channel.
 * \param newchan		New asterisk channel.
 * \retval 0			Always returns 0.
 */
static int CHAN_USB_FIXUP(struct ast_channel *oldchan, struct ast_channel *newchan)
{
	chan_usb_pvt *o = ast_channel_tech_pvt(newchan);

	ast_log(LOG_WARNING, "Channel %s: Fixup received.\n", o->name);
	o->owner = newchan;
	return 0;
}

/*!
 * \brief Asterisk setoption function.
 * \param chan			Asterisk channel.
 * \param option		Option.
 * \param data			Data.
 * \param datalen		Data length.
 * \retval 0			If successful.
 * \retval -1			If failed.
 */
static int CHAN_USB_SETOPTION(struct ast_channel *chan, int option, void *data, int datalen)
{
	char *cp;
	chan_usb_pvt *o = ast_channel_tech_pvt(chan);

	/* all supported options require data */
	if (!data || (datalen < 1)) {
		errno = EINVAL;
		return -1;
	}

	switch (option) {
	case AST_OPTION_TONE_VERIFY:
		cp = data;
		switch (*cp) {
		case 1:
			ast_log(LOG_NOTICE, "Channel %s: Set option TONE VERIFY, mode: OFF(0).\n", o->name);
			o->usedtmf = 1;
			break;
		case 2:
			ast_log(LOG_NOTICE, "Channel %s: Set option TONE VERIFY, mode: MUTECONF/MAX(2).\n", o->name);
			o->usedtmf = 1;
			break;
		case 3:
			ast_log(LOG_NOTICE, "Channel %s: Set option TONE VERIFY, mode: DISABLE DETECT(3).\n", o->name);
			o->usedtmf = 0;
			break;
		default:
			ast_log(LOG_NOTICE, "Channel %s: Set option TONE VERIFY, mode: OFF(0).\n", o->name);
			o->usedtmf = 1;
			break;
		}
		break;
	}
	errno = 0;
	return 0;
}

/*!
 * \brief Process Asterisk CLI request to key radio.
 * \param fd			Asterisk CLI fd
 * \param argc			Number of arguments
 * \param argv			Arguments
 * \return	CLI success, showusage, or failure.
 */
static int console_key(int fd, int argc, const char *const *argv)
{
	chan_usb_pvt *o = find_desc(CHAN_USB_ACTIVE);

	if (argc != 2) {
		return RESULT_SHOWUSAGE;
	}
	o->txtestkey = 1;
	kickptt(o);
	return RESULT_SUCCESS;
}

/*!
 * \brief Process Asterisk CLI request to unkey radio.
 * \param fd			Asterisk CLI fd
 * \param argc			Number of arguments
 * \param argv			Arguments
 * \return	CLI success, showusage, or failure.
 */
static int console_unkey(int fd, int argc, const char *const *argv)
{
	chan_usb_pvt *o = find_desc(CHAN_USB_ACTIVE);

	if (argc != 2) {
		return RESULT_SHOWUSAGE;
	}
	o->txtestkey = 0;
	kickptt(o);
	return RESULT_SUCCESS;
}

/*!
 * \brief Update the tune settings to the configuration file.
 * \param config	The (opened) config to use
 * \param filename	The configuration file being updated.
 * \param category	The category being updated (e.g. "12345").
 * \param variable	The variable being updated (e.g. "rxboost").
 * \param value		The value being updated (e.g. "yes").
 * \retval 0		If successful.
 * \retval -1		If unsuccessful.
 */
static int tune_variable_update(struct ast_config *config, const char *filename, struct ast_category *category,
	const char *variable, const char *value)
{
	int res;
	struct ast_variable *v, *var = NULL;

	/* ast_variable_retrieve, but returning the variable struct */
	for (v = ast_variable_browse(config, ast_category_get_name(category)); v; v = v->next) {
		if (!strcasecmp(variable, v->name)) {
			var = v;
		}
	}

	if (var && !strcmp(var->value, value)) {
		/* no need to update a matching value */
		return 0;
	}

	if (var && !var->inherited) {
		/* the variable is defined and not inherited from a template category */
		res = ast_variable_update(category, variable, value, var->value, var->object);
		if (res == 0) {
			return 0;
		}
	}

	/* create and add the variable / value to the category */
	var = ast_variable_new(variable, value, filename);
	if (var == NULL) {
		return -1;
	}

	/* and append */
	ast_variable_append(category, var);
	return 0;
}

/*!
 * \brief Load configuration.
 * \param reload		Flag to indicate if we are reloading.
 * \return				Success or failure.
 */
static int load_config(int reload)
{
	struct ast_config *cfg = NULL;
	char *ctg = NULL;
	const char *val;
	struct ast_flags zeroflag = { reload ? CONFIG_FLAG_FILEUNCHANGED : 0 };

	/* load config file */
	if (!(cfg = ast_config_load(CONFIG, zeroflag))) {
		ast_log(LOG_NOTICE, "Unable to load config %s.\n", CONFIG);
		return AST_MODULE_LOAD_DECLINE;
	} else if (cfg == CONFIG_STATUS_FILEUNCHANGED) {
		ast_log(LOG_NOTICE, "Config file %s unchanged, skipping.\n", CONFIG);
		return 0;
	} else if (cfg == CONFIG_STATUS_FILEINVALID) {
		ast_log(LOG_ERROR, "Config file %s is in an invalid format. Aborting.\n", CONFIG);
		return -1;
	}

	/* store the configuration */
	do {
		store_config(cfg, ctg);
	} while ((ctg = ast_category_browse(cfg, ctg)) != NULL);

	/* load parallel port information */
	ppfd = -1;
	pbase = 0;
	val = ast_variable_retrieve(cfg, "general", "pport");
	if (val) {
		ast_copy_string(pport, val, sizeof(pport));
	} else {
		strcpy(pport, PP_PORT);
	}
	val = ast_variable_retrieve(cfg, "general", "pbase");
	if (val) {
		pbase = strtoul(val, NULL, 0);
	}
	if (!pbase) {
		pbase = PP_IOPORT;
	}
	ast_radio_load_parallel_port(&haspp, &ppfd, &pbase, pport, reload);
	ast_config_destroy(cfg);
	return 0;
}

/*!
 * \brief Turns integer response to char CLI response
 * \param r				Response.
 * \return	CLI success, showusage, or failure.
 */
static char *res2cli(int r)
{
	switch (r) {
	case RESULT_SUCCESS:
		return CLI_SUCCESS;
	case RESULT_SHOWUSAGE:
		return CLI_SHOWUSAGE;
	default:
		return CLI_FAILURE;
	}
}

#endif /* CHAN_USB_COMMON_H */
