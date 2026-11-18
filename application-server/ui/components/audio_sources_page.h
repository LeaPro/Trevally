/**
 * @file audio_sources_page.h
 * @brief The audio sources page, opened from the cast icon on the home screen: a picker with a Bluetooth row
 *        (a shortcut to the Bluetooth settings page) and a row for every configured streaming service.
 *        Choosing a service enables it right away; the check follows the enabled source.
 */

#ifndef AUDIO_SOURCES_PAGE_H
#define AUDIO_SOURCES_PAGE_H

/** Pushes the page over the current screen. */
void audio_sources_page_open();

/** Pushes the Streaming settings page: the same list without the Bluetooth row. Choosing a service enables it
 *  and stays on the page, where the check follows the enabled service. */
void audio_sources_streaming_page_open();

#endif /* AUDIO_SOURCES_PAGE_H */
