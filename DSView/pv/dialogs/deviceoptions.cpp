/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <dreamsourcelab@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */


#include "deviceoptions.h"

#include <boost/foreach.hpp>

#include <QFormLayout>
#include <QListWidget>
#include <QMessageBox>

#include <pv/prop/property.h>

using namespace boost;
using namespace std;

namespace pv {
namespace dialogs {

DeviceOptions::DeviceOptions(QWidget *parent, shared_ptr<pv::device::DevInst> dev_inst) :
	QDialog(parent),
    _dev_inst(dev_inst),
	_layout(this),
	_button_box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
		Qt::Horizontal, this),
    _device_options_binding(_dev_inst->dev_inst())
{
	setWindowTitle(tr("Configure Device"));
	setLayout(&_layout);

    _props_box = new QGroupBox(tr("Mode"), this);
    _props_box->setLayout(&_props_box_layout);
    _props_box_layout.addWidget(get_property_form());
    _layout.addWidget(_props_box);

    if (_dev_inst->dev_inst()->mode != DSO) {
        _probes_box = new QGroupBox(tr("Channels"), this);
        setup_probes();
        _probes_box->setLayout(&_probes_box_layout);
        _layout.addWidget(_probes_box);
    } else {
        _config_button = new QPushButton(tr("Zero Adjustment"), this);
        _layout.addWidget(_config_button);
        connect(_config_button, SIGNAL(clicked()), this, SLOT(zero_adj()));
    }

    _layout.addStretch(1);
	_layout.addWidget(&_button_box);

    connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));
    connect(&_button_box, SIGNAL(rejected()), this, SLOT(reject()));
}

void DeviceOptions::accept()
{
	using namespace Qt;

	QDialog::accept();

	// Commit the properties
	const vector< boost::shared_ptr<pv::prop::Property> > &properties =
		_device_options_binding.properties();
	BOOST_FOREACH(boost::shared_ptr<pv::prop::Property> p, properties) {
		assert(p);
		p->commit();
	}

    // Commit the probes
    if (_dev_inst->dev_inst()->mode != DSO) {
        int index = 0;
        for (const GSList *l = _dev_inst->dev_inst()->channels; l; l = l->next) {
            sr_channel *const probe = (sr_channel*)l->data;
            assert(probe);

            probe->enabled = (_probes_checkBox_list.at(index)->checkState() == Qt::Checked);
            index++;
        }
    }
}

void DeviceOptions::reject()
{
    using namespace Qt;
    QDialog::reject();

}

QWidget* DeviceOptions::get_property_form()
{
	QWidget *const form = new QWidget(this);
	QFormLayout *const layout = new QFormLayout(form);
	form->setLayout(layout);

	const vector< boost::shared_ptr<pv::prop::Property> > &properties =
		_device_options_binding.properties();
	BOOST_FOREACH(boost::shared_ptr<pv::prop::Property> p, properties)
	{
		assert(p);
		const QString label = p->labeled_widget() ? QString() : p->name();
		layout->addRow(label, p->get_widget(form));
	}

	return form;
}

void DeviceOptions::setup_probes()
{
	using namespace Qt;

    int row = 0, col = 0;
    int index = 0;

    while(_probes_box_layout.count() > 0)
    {
        //remove Widgets in QLayoutGrid
        QWidget* widget = _probes_box_layout.itemAt(0)->widget();
        _probes_box_layout.removeWidget(widget);
        delete widget;
    }
    _probes_label_list.clear();
    _probes_checkBox_list.clear();

    for (const GSList *l = _dev_inst->dev_inst()->channels; l; l = l->next) {
		sr_channel *const probe = (sr_channel*)l->data;
		assert(probe);

        QLabel *probe_label = new QLabel(QString::number(probe->index), this);
        QCheckBox *probe_checkBox = new QCheckBox(this);
        probe_checkBox->setCheckState(probe->enabled ? Qt::Checked : Qt::Unchecked);
        _probes_box_layout.addWidget(probe_label, row * 2, col);
        _probes_box_layout.addWidget(probe_checkBox, row * 2 + 1, col);
        _probes_label_list.push_back(probe_label);
        _probes_checkBox_list.push_back(probe_checkBox);

        index++;
        col = index % 8;
        row = index / 8;
	}

    QPushButton *_enable_all_probes = new QPushButton("Enable All", this);
    QPushButton *_disable_all_probes = new QPushButton("Disable All", this);

    connect(_enable_all_probes, SIGNAL(clicked()),
        this, SLOT(enable_all_probes()));
    connect(_disable_all_probes, SIGNAL(clicked()),
        this, SLOT(disable_all_probes()));

    _probes_box_layout.addWidget(_enable_all_probes, (row + 1) * 2, 0, 1, 4);
    _probes_box_layout.addWidget(_disable_all_probes, (row + 1) * 2, 4, 1, 4);
}

void DeviceOptions::set_all_probes(bool set)
{
    QVector<QCheckBox *>::iterator i = _probes_checkBox_list.begin();
    while(i != _probes_checkBox_list.end()) {
        (*i)->setCheckState(set ? Qt::Checked : Qt::Unchecked);
        i++;
    }
}

void DeviceOptions::enable_all_probes()
{
	set_all_probes(true);
}

void DeviceOptions::disable_all_probes()
{
	set_all_probes(false);
}

void DeviceOptions::zero_adj()
{
    using namespace Qt;
    QDialog::reject();

    QMessageBox msg(this);
    msg.setText("Information");
    msg.setInformativeText("Zero adjustment program will be started. This may take a few minutes!");
    //msg.setStandardButtons(QMessageBox::);
    msg.addButton(tr("Ok"), QMessageBox::AcceptRole);
    msg.addButton(tr("Cancel"), QMessageBox::RejectRole);
    msg.setIcon(QMessageBox::Information);
    int ret = msg.exec();
    if ( ret == QMessageBox::AcceptRole) {
        _dev_inst->set_config(NULL, NULL, SR_CONF_ZERO, g_variant_new_boolean(true));
    }
}

} // namespace dialogs
} // namespace pv
