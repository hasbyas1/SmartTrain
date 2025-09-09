import React from "react";
import Sidebar from "../components/AdminSidebar";
import Navbar from "../components/AdminNavbar";
import LiveCamPreview from "../components/LiveCamPreview";
import { useState } from "react";

export default function AdminDashboard() {
  const [isOn, setIsOn] = useState(false);
  return (
    <div className="flex min-h-screen bg-gray-100">
      {/* Sidebar */}
      <Sidebar />

      {/* Main content */}
      <div className="md:ml-72 flex flex-col flex-1">
        <Navbar />

        <main className="p-4 md:p-6 lg:p-8 grid grid-cols-1 lg:grid-cols-2 gap-6">
          {/* Live Camera From Train */}
          <div className="bg-white rounded-xl shadow p-4">
            <h3 className="mb-2 font-semibold">Live Camera From Train</h3>
            <div className="aspect-video bg-gray-200 rounded-lg flex items-center justify-center">
              <LiveCamPreview
                 wsUrl={`${process.env.REACT_APP_API_URL}/stream`}
                autoPlay={true}
                controls={true}
              />
            </div>
          </div>

          {/* Live Camera From Palang Perlintasan */}
          <div className="bg-white rounded-xl shadow p-4">
            <h3 className="mb-2 font-semibold">
              Live Camera From Palang Perlintasan
            </h3>
            <div className="aspect-video bg-gray-200 rounded-lg flex items-center justify-center">
              <LiveCamPreview
                 wsUrl={`${process.env.REACT_APP_API_URL}/stream`}
                autoPlay={true}
                controls={true}
              />
            </div>
          </div>

          {/* Train Map */}
          <div className="bg-white rounded-xl shadow p-4 lg:col-span-2">
            <h3 className="mb-2 font-semibold">Train Map</h3>
            <div className="w-full py-4 bg-gray-200 rounded-lg flex items-center justify-center">
              <img
                src="/img/map.png"
                alt="Login Illustration"
                className="object-scale-down"
              ></img>
            </div>
          </div>

          {/* Palang Perlintasan */}
          <div className="bg-white rounded-xl shadow p-4 flex items-center justify-between">
            <h3 className="font-semibold">Palang Perlintasan 1</h3>
            <button
              onClick={() => setIsOn(!isOn)}
              className={`relative inline-flex h-12 w-24 items-center rounded-full transition-colors duration-300 focus:outline-none 
        ${isOn ? "bg-[#EB2525]" : "bg-gray-300"}`}
            >
              <span
                className={`inline-block h-8 w-8 transform rounded-full bg-white transition-transform duration-300 
          ${isOn ? "translate-x-14" : "translate-x-1"}`}
              />
            </button>
          </div>
          <div className="bg-white rounded-xl shadow p-4 flex items-center justify-between">
            <h3 className="font-semibold">Palang Perlintasan 2</h3>
            <button
              onClick={() => setIsOn(!isOn)}
              className={`relative inline-flex h-12 w-24 items-center rounded-full transition-colors duration-300 focus:outline-none 
        ${isOn ? "bg-[#EB2525]" : "bg-gray-300"}`}
            >
              <span
                className={`inline-block h-8 w-8 transform rounded-full bg-white transition-transform duration-300 
          ${isOn ? "translate-x-14" : "translate-x-1"}`}
              />
            </button>
          </div>
        </main>
      </div>
    </div>
  );
}
