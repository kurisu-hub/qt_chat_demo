#include "q.h"
#include "ui_q.h"

q::q(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::q)
{
    ui->setupUi(this);
}

q::~q()
{
    delete ui;
}
