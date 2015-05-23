#ifndef FFTRESIZETHREAD_H
#define FFTRESIZETHREAD_H

#include <QThread>
#include <QMutex>

#include "sigproc.h"

class FFTResizeThread : public QThread
{
    Q_OBJECT

    sigproc::FFTwrapper* m_fft;   // The FFT transformer

    int m_size_resizing;// The size which is in preparation by FFTResizeThread
    int m_size_todo;    // The next size which has to be done by FFTResizeThread asap

    void run(); //Q_DECL_OVERRIDE

signals:
    void fftResizing(int prevSize, int newSize);
    void fftResized(int prevSize, int newSize);

public:
    FFTResizeThread(sigproc::FFTwrapper* fft, QObject* parent);

    void resize(int newsize); // Entry point

    mutable QMutex m_mutex_resizing;      // To protect the access to the FFT transformer
    mutable QMutex m_mutex_changingsizes; // To protect the access to the size variables above
};

#endif // FFTRESIZETHREAD_H