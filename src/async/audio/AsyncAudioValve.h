/**
@file	 AsyncAudioValve.h
@brief   This file contains a class that implements a "valve" for audio.
@author  Tobias Blomberg / SM0SVX
@date	 2006-07-08

\verbatim
Async - A library for programming event driven applications
Copyright (C) 2004-2008  Tobias Blomberg / SM0SVX

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\endverbatim
*/


#ifndef ASYNC_AUDIO_VALVE_INCLUDED
#define ASYNC_AUDIO_VALVE_INCLUDED


/****************************************************************************
 *
 * System Includes
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Project Includes
 *
 ****************************************************************************/

#include <AsyncAudioSink.h>
#include <AsyncAudioSource.h>
#include <AsyncAudioFifo.h>
#include <SigCAudioSource.h>



/****************************************************************************
 *
 * Local Includes
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Forward declarations
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Namespace
 *
 ****************************************************************************/

namespace Async
{


/****************************************************************************
 *
 * Forward declarations of classes inside of the declared namespace
 *
 ****************************************************************************/

  

/****************************************************************************
 *
 * Defines & typedefs
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Exported Global Variables
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Class definitions
 *
 ****************************************************************************/

/**
@brief	Implements a "valve" for audio
@author Tobias Blomberg / SM0SVX
@date   2006-07-08

This class implements a "valve" for audio. That is, the audio stream can be
turned on or off. It's named "valve" since the whole Async audio concept is
called audio pipe.
*/
class AudioValve : public SigC::Object, public Async::AudioSink, public Async::AudioSource
{
  public:
    /**
     * @brief 	Default constuctor
     */
    explicit AudioValve(void)
      : stream_state(STREAM_IDLE), fifo(64), block_when_closed(false),
        is_open(true), is_blocking(false)
    {
      AudioSource::setHandler(&fifo);
      sigsrc.registerSink(&fifo);
      sigsrc.sigRequestSamples.connect(slot(*this, &AudioValve::onRequestSamples));
      sigsrc.sigAllSamplesFlushed.connect(slot(*this, &AudioValve::onAllSamplesFlushed));
    }
  
    /**
     * @brief 	Destructor
     */
    ~AudioValve(void)
    {
      AudioSource::clearHandler();
    }
    
    /**
     * @brief 	Open or close the valve
     * @param 	do_open If \em true the valve is open or else it's closed
     *
     * This function is used to open or close the audio valve. When the valve
     * is closed, the connected sink is flushed.
     */
    void setOpen(bool do_open)
    {
      if (is_open == do_open)
      {
      	return;
      }
      
      is_open = do_open;
      
      if (do_open)
      {
        if (is_blocking)
        {
          /* When the valve is opened and the source is blocked, we have */
          /* to request samples from the source to put the sink into     */
          /* active state.                                               */
          sourceRequestSamples(fifo.spaceAvail());
        }
      }
      else
      {
        if (!block_when_closed)
        {
          /* request samples from the source until it is empty */
          readSourceEmpty();
        }
        switch (stream_state)
        {
        case STREAM_FLUSHING:
	  stream_state = STREAM_IDLE;
	  is_blocking = false;
	  sourceAllSamplesFlushed();
	  break;
        case STREAM_ACTIVE:
	  stream_state = STREAM_FLUSHING;
      	  sigsrc.flushSamples();
      	  break;
        case STREAM_IDLE:
          break;
	}
      }
    }

    /**
     * @brief 	Setup audio stream blocking when valve is closed
     * @param 	block_when_closed When \em true, block the incoming audio
     *	      	stream when the valve is closed. When \em false, incoming
     *	      	audio is thrown away when the valve is closed.
     *
     * Use this method to set if the valve should block or drop the incoming
     * audio stream when the valve is closed.
     */
    void setBlockWhenClosed(bool block_when_closed)
    {
      if (block_when_closed == this->block_when_closed)
      {
      	return;
      }
      
      this->block_when_closed = block_when_closed;
      
      if (!is_open && !block_when_closed)
      {
        /* request samples from the source until it is empty */
        readSourceEmpty();
      }
    }
    
    /**
     * @brief 	Check if the valve is open
     * @returns Return \em true if the valve is open or else \em false
     */
    bool isOpen(void) const
    {
      return is_open;
    }
    
    /**
     * @brief 	Check if the valve is idle
     * @returns Return \em true if the valve is idle or else \em false
     */
    bool isIdle(void) const
    {
      return (stream_state == STREAM_IDLE);
    }
  
    /**
     * @brief 	Write samples into the valve
     * @param 	samples The buffer containing the samples
     * @param 	count The number of samples in the buffer
     * @return	Returns the number of samples that has been taken care of
     *
     * This function is used to write audio into the valve. If it
     * returns 0, no more samples could be written.
     * If the returned number of written samples is lower than the count
     * parameter value, the sink is not ready to accept more samples.
     * In this case, the audio source requires sample buffering to temporarily
     * store samples that are not immediately accepted by the sink.
     * The writeSamples function should be called on source buffer updates
     * and after a source output request has been received through the
     * requestSamples function.
     * This function is normally only called from a connected source object.
     */
    int writeSamples(const float *samples, int count)
    {
      stream_state = STREAM_ACTIVE;
      if (is_open)
      {
      	written = sigsrc.writeSamples(samples, count);
      }
      else
      {
        written = block_when_closed ? 0 : count;
      }
      is_blocking |= (written < count);
      return written;
    }
    
    /**
     * @brief 	Tell the valve to flush the previously written samples
     *
     * This function is used to tell the valve to flush previously written
     * samples. When done flushing, the valve will call the
     * allSamplesFlushed function in the connected source object.
     * This function is normally only called from a connected source object.
     */
    void flushSamples(void)
    {
      if (is_open)
      {
      	stream_state = STREAM_FLUSHING;
      	sigsrc.flushSamples();
      }
      else
      {
	stream_state = STREAM_IDLE;
	is_blocking = false;
      	sourceAllSamplesFlushed();
      }
    }

    
  private:
    AudioValve(const AudioValve&);
    AudioValve& operator=(const AudioValve&);

    void readSourceEmpty(void)
    {
      while (stream_state == STREAM_ACTIVE)
      {
        written = 0;
        sourceRequestSamples(64);
        if (written == 0) break;
      }
    }

    void onRequestSamples(int count)
    {
      if (is_open)
      {
      	sourceRequestSamples(count);
      }
    }
    
    void onAllSamplesFlushed(void)
    {
      bool was_flushing = (stream_state == STREAM_FLUSHING);
      stream_state = STREAM_IDLE;
      is_blocking = false;
      if (is_open && was_flushing)
      {
      	sourceAllSamplesFlushed();
      }
    }

    typedef enum
    {
      STREAM_IDLE, STREAM_ACTIVE, STREAM_FLUSHING
    } StreamState;

    StreamState stream_state;
    AudioFifo fifo;
    SigCAudioSource sigsrc;
    bool block_when_closed;
    bool is_open;
    bool is_blocking;
    int written;
    
};  /* class AudioValve */


} /* namespace */

#endif /* ASYNC_AUDIO_VALVE_INCLUDED */



/*
 * This file has not been truncated
 */

